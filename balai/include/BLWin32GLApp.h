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
 * @file	BLWin32GLApp.h
 * @desc    win32 application base class using OpenGL ES renderer(via Adreno SDK)
 * @author	andre chen
 * @history	2011/12/30 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_ALLOW_PLATFORM_APPLICATION_INCLUDE
#error "Do NOT include 'BLWin32GLApp.h' directly, #include 'BLApplication.h' instead"
#endif

#ifndef BL_WIN32GL_APPLIATION_H
#define BL_WIN32GL_APPLIATION_H

#ifdef BL_ADRENO_SDK_EGL

#include <EGL/egl.h>

namespace mlabs { namespace balai { namespace framework {

typedef class Win32GLApp : protected Application
{
	// disable copy ctor and assignment operator
	BL_NO_COPY_ALLOW(Win32GLApp);

	// EGL
	EGLDisplay  eglDisplay_;
    EGLContext  eglContextGL_;
    EGLSurface  eglSurface_;

	// window handle
    HWND   hWnd_;

	// window positions
	uint16 left_;
	uint16 top_;

protected:
    char const* window_title_;

	// implement class may modify width and height
	uint16 width_;
	uint16 height_;

	uint8  colorBits_;
	uint8  depthBits_;
    uint8  stencilBits_;
    uint8  vsync_on_;

	// grant access to derived classes only
	Win32GLApp();
    ~Win32GLApp() { sWin32App = NULL; } // non-public, non-virtual

private:
    uint8 active_;
	uint8 minimized_;
	uint8 maxmized_; // ?

	// internal usage methods...
	bool CreateRenderContext_();
	void DestroyRenderContext_();

	// create then run
	bool Create(void* hInstance);
    void Destroy() {}
	int  Run();

	// methods you should overwrite
	virtual bool Initialize()				 = 0;
	virtual bool FrameMove(float updateTime) = 0;
	virtual bool Render()					 = 0;
	virtual void Cleanup()					 = 0;
	virtual bool MsgProc(HWND, UINT, WPARAM, LPARAM) { return false; }

	 // touch...
    virtual void TouchBegan(uint32, uint32, float, float) {}
    virtual void TouchMoved(uint32, uint32, float, float, float, float) {}
    virtual void TouchEnded(uint32, uint32, float, float) {}
//	virtual void TouchCancelled(uint32 serialNo, uint32 tapCount, float x, float y) {}

private:
	// Default windows procedure
	LRESULT BuiltinWinProc(HWND, UINT, WPARAM, LPARAM);

	// pointer to launch MsgProc
	static Win32GLApp* sWin32App; // msg hook

	// WinMsg handler
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        return sWin32App->BuiltinWinProc(hWnd, uMsg, wParam, lParam);
	}
} BaseApp;

}}}
#else

#include "SDL.h" // Simple DirectMedia Layer

namespace mlabs { namespace balai { namespace framework {

typedef class Win32GLApp : protected Application
{
    // disable copy ctor and assignment operator
    BL_NO_COPY_ALLOW(Win32GLApp);

    // SDL
    SDL_Window*   window_;
    SDL_GLContext glContext_;

    // window positions
    uint16 left_;
    uint16 top_;

    // application icon, pixel format = RGB565 
    virtual uint16* LoadIconData_(int& /*width*/, int& /*height*/) {
        // width, height maybe 16, 32 or 64. (128 does not work)
        return NULL; // 
    }
    virtual void FreeIconData_(void* /*pixels*/, int /*width*/, int /*height*/) {}

protected:
    char const* window_title_;
    HWND WindowsHandle_() const;

    // implement class may modify width and height
    uint16 width_;
    uint16 height_;

    uint8  colorBits_;
    uint8  depthBits_;
    uint8  stencilBits_;
    uint8  vsync_on_;
    //uint8  fullscreen_;
    uint8  gl_major_version_;
    uint8  gl_minor_version_;
    uint8  gl_debug_on_; // GL 4.3+

    // grant access to derived classes only
    Win32GLApp();
    ~Win32GLApp(); // non-public, non-virtual

private:
    uint8 active_;

    // create then run
    bool Create(void* hInstance);
    void Destroy() {}
    int  Run();

    void SDL_MessageHandlerThread_();

    // methods you should overwrite
    virtual bool Initialize()				 = 0;
    virtual bool FrameMove(float updateTime) = 0;
    virtual bool Render()					 = 0;
    virtual void Cleanup()					 = 0;
    virtual bool SDLEventHandler(SDL_Event const&) { return false; }

    // touch...
    virtual void TouchBegan(uint32, uint32, float, float) {}
    virtual void TouchMoved(uint32, uint32, float, float, float, float) {}
    virtual void TouchEnded(uint32, uint32, float, float) {}
//  virtual void TouchCancelled(uint32 serialNo, uint32 tapCount, float x, float y) {}

private:
    // Default windows procedure
    bool BuiltinEventHandler_(SDL_Event const& sdl_event);

} BaseApp;

}}}

#endif

#endif // BL_WIN32GL_APPLIATION_H