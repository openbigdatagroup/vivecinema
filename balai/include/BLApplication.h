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
 * @file	BLApplication.h
 * @desc    application base class
 * @author	andre chen
 * @history	2011/12/30 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_APPLICATION_H
#define BL_APPLICATION_H

#include "BLCore.h"
// #include <cmath>

namespace mlabs { namespace balai { namespace framework {

//-----------------------------------------------------------------------------
// base application class
//-----------------------------------------------------------------------------
class Application
{
	double	appTime_;
	float	frameTime_;
	float	fps_;
	float	timeScale_;
	uint32	pause_;

protected:
	Application():
		 appTime_(0.0),frameTime_(0.0f),fps_(0.0f),timeScale_(1.0f),pause_(0) {}
	virtual ~Application() {}

	// call every frame - return update time(scaled frame time) in seconds
	float UpdateTime_() {
		static double one_second_counter_ = 0.0;
		static uint32 frame_count_		  = 0;

		// calculate frame time
		double const curTime = system::GetTime();
		if (curTime>appTime_) {
			frameTime_ = (float) (curTime - appTime_);
			appTime_   = curTime;
		}
		else {
			// note on IEEE 754(double precision of floating point format, S 11E 52M) :
			// to keep app run in 60 fps, frame time (dt) = .01666667(1/60), approximate to 2^-6
			//
			// the system is guaranteed to be OK to last 70368744177664(2^46) secs,
			// that's 814,453,057 days 14 hrs 41 mins and 4 secs!
			//
			// Also note that IDirect3D::CreateDevice() will push FPU state to single precision.
			// single precision has only 23 bits mantissa. And it degenerates to 131,072(2^16) secs,
			// or 1 day 12 hrs 24 mins and 32 secs. To fix this, use D3DCREATE_FPU_PRESERVE option 
			// when creating the D3D device. Microsoft's docs mention an unspecified performance hit 
			// but I'm wondering if the impact is noticeable.

			// to reset timer, must make sure that all subsystems have been notified.
			appTime_ = system::GetTime(true);
//			frameTime_ = 0.0f; // as last frame
		}

		// fps statistics
		++frame_count_;
		one_second_counter_ += frameTime_;
		if (one_second_counter_>1.0) {
			fps_ = (float)(frame_count_/one_second_counter_);
//			one_second_counter_ -= std::floor(one_second_counter_); // keep resdue?
			one_second_counter_ = 0.0f;
			frame_count_		= 0;
		}

		return (0==pause_) ? (frameTime_*timeScale_):0.0f;
	}

	virtual void OnPause_(uint32 /*pause*/) {}

public:
	// called by main/WinMain
	virtual void ParseCommandLine(int /*argc*/, char* /*argv*/[]) {}
	virtual bool Create(void* param=0) = 0;
    virtual void Destroy() = 0; // to be paired with Create() 2012.11.02
    virtual void OnQuit() {}  // last function to be called

#if defined(BL_OS_ANDROID) || defined(BL_OS_APPLE_iOS)
	// methods plug into platform app framework
	virtual bool FrameUpdate() = 0;

	// type : 0(fusion, like rotation vector), 1(accelerometer), 2(gyroscope), 3(campass)
	virtual void OnSensorData(int /*type*/, float /*x*/, float /*y*/, float /*z*/, float /*elapsed_time*/) {}
#else
	virtual int Run() = 0;
#endif

	virtual void OnSurfaceResized(uint32 /*width*/, uint32 /*height*/) {}

	// touches callbacks(0.0f <= x, y <= 1.0f, x towards "right" while y towards "up")
	virtual void TouchBegan(uint32 touchId, uint32 tapCount, float x, float y)					   = 0;
	virtual void TouchMoved(uint32 touchId, uint32 tapCount, float x, float y, float dx, float dy) = 0;
	virtual void TouchEnded(uint32 touchId, uint32 tapCount, float x, float y)					   = 0;
	virtual void TouchCancelled(uint32 touchId, uint32 tapCount, float x, float y) {
		TouchEnded(touchId, tapCount, x, y);
	}

	// time
	double GetAppTime() const	 { return appTime_;   }
	float  GetFrameTime() const	 { return frameTime_; }
	float  GetFPS() const		 { return fps_;       }
	float  GetTimeScale() const	 { return timeScale_; }
	void   SetTimeScale(float s) { timeScale_ = s;	  } 
	bool   IsPaused() const      { return pause_!=0; }
	
	// Pause - return last state
	bool Pause(bool pause) {
		bool last_state = (pause_!=0);
		if (pause!=last_state) {
			pause_ = pause;
			if (!pause)
				appTime_ = system::GetTime();
			OnPause_(pause_);
		}
		return last_state;
	}
	bool Pause() {
		pause_ = (pause_==0);
		OnPause_(pause_);
		if (pause_!=0)
			return false; // last state
		appTime_ = system::GetTime();
		return true; // last state
	}

	// statics
	static Application& GetInstance();
};

}}}


//-----------------------------------------------------------------------------
#define DECLARE_APPLICATION_CLASS friend class Application
#define IMPLEMENT_APPLICATION_CLASS(ApplicationClass) \
Application& Application::GetInstance() {	\
	static ApplicationClass app_; \
	return app_; \
}


//-----------------------------------------------------------------------------
// platform application class - IBaseApp
//-----------------------------------------------------------------------------
#define BL_ALLOW_PLATFORM_APPLICATION_INCLUDE
#if defined(BL_OS_WIN32) || defined(BL_OS_WIN64)
    #ifdef BL_RENDERER_D3D
	    #include "BLWin32D3DApp.h"
    #else
        #include "BLWin32GLApp.h"
    #endif
#elif defined(BL_OS_ANDROID)
	#include "BLAndroidApp.h"
//
// more to come...
//
#else
	#error "Platform app class not define!!!"
#endif
#undef BL_ALLOW_PLATFORM_APPLICATION_INCLUDE

#endif // BL_APPLICATION_H
