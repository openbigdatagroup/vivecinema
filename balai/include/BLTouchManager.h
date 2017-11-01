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
 * @file    BLTouchManager.h
 * @author  andre chen
 * @history 2012/01/30 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_TOUCH_MANAGER_H
#define BL_TOUCH_MANAGER_H

#include "BLCore.h"

namespace mlabs { namespace balai { namespace input {

struct Touch {
    // coordinate system :
    //  (0,0)------(1,0)
    //    |          |
    //  (0,1)------(1,1)
    //
    struct Point { float x, y; };
    Point   InitPosition;
    Point   CurrPosition;
    Point   PrevPosition;
    mutable Point PinPosition;
	float	TapTime;  // tap or release time
    uint16  TapCount; // number of taps happen before touchBegin
	uint16	Status;	  // 0:release, 1:touch, 2:move

	Touch():TapTime(-1.0f),TapCount(0),Status(0) {}
	void Reset() {
		TapTime = -1.0f;
		TapCount = Status = 0;
	}
};

class TouchManager
{
    BL_NO_COPY_ALLOW(TouchManager);

    //
    /* use mutex to prevent race condition? */
    //
    enum { MAX_MULTIPLE_TOUCHES = 16 }; // 16 fingers!
    Touch   touches_[MAX_MULTIPLE_TOUCHES];
    float   aspect_ratio_; // x/y
	float	double_tap_timeout_;
	float	double_tap_cabnorm_; // FIX-ME : this should be respect to screen size and resolution!

	TouchManager():aspect_ratio_(1.0f),double_tap_timeout_(0.4f),double_tap_cabnorm_(0.2f) {}
    ~TouchManager() {}

public:
    // plug into view/surface/window - so don't call me baby~
    bool TouchBegan(int, float x, float y);
    bool TouchMoved(int, float x, float y, float xPrev=0.0f, float yPrev=0.0f);
    bool TouchEnded(int, float x, float y);
    void TouchCancelled();

    int GetTotalTouches() const {
        int count = 0;
        for (int i=0; i<MAX_MULTIPLE_TOUCHES; ++i) {
            if (0!=touches_[i].Status)
                ++count;
        }
        return count;
    }

	// set touch region
    void SetTouchRegion(int width, int height) {
		aspect_ratio_ = width/(float)height;
		// adjust double_tap_cabnorm_ !?
	}

	// set double(and up) tap timeout
	void SetDoubleTapTimeout(float period) { if (period>0.0f) double_tap_timeout_ = period; };

	// set double tap threshold ???
	void SetDoubleTapDistanceThreshold(float norm) { double_tap_cabnorm_ = norm; }

    // retrieve
    Touch const* GetTouch(uint32 index) const {
        if (index<MAX_MULTIPLE_TOUCHES) {
            return (touches_+index);
        }
        return NULL;
    }

    // gesture analysis
    bool GetGestureFlick(float& dx, float& dy, uint32 index=0) const {
        if (index<MAX_MULTIPLE_TOUCHES) {
            Touch const& t = touches_[index];
			if (t.Status>1) {
				dx = t.CurrPosition.x - t.PinPosition.x;
				dy = t.CurrPosition.y - t.PinPosition.y;
				t.PinPosition = t.CurrPosition;
				return true;
			}
        }
        return false;
    }

	// 2 fingers
    bool GetGesturePinch(float& scale, float& rotate);

	// meyer's singleton
    static TouchManager& GetInstance() {
        static TouchManager inst_;
        return inst_;
    }
};

}}}

#endif