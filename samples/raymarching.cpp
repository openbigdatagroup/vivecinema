/* 
 * MIT license
 *
 * Copyright (c) 2017 SFG(Star-Formation Group), Physics Dept., CUHK.
 * All rights reserved.
 *
 * file    raymarching.cpp
 * author  andre chen, andre.hl.chen@gmail.com
 * history 2017/10/06 created
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "SFG/SFGVoxelViewer.h"
#include "BLApplication.h"
#include "BLCStdMemory.h"
#include "BLPrimitives.h"
#include "BLFont.h"
#include "BLCamera.h"

BL_CSTD_MEMMGR_IMPLMENTATION(2<<20, 1<<20);

using namespace sfg;
using namespace mlabs::balai::graphics;
using namespace mlabs::balai::math;

void DrawSomething()
{
    Primitives& prim = Primitives::GetInstance();
    if (prim.BeginDraw(NULL, GFXPT_QUADLIST)) {
        prim.SetColor(Color::Red);
        prim.AddVertex( 1.0f,-1.0f, 1.0f);
        prim.AddVertex( 1.0f,-1.0f,-1.0f);
        prim.AddVertex( 1.0f, 1.0f,-1.0f);
        prim.AddVertex( 1.0f, 1.0f, 1.0f);

        prim.SetColor(Color::Green);
        prim.AddVertex( 1.0f, 1.0f, 1.0f);
        prim.AddVertex( 1.0f, 1.0f,-1.0f);
        prim.AddVertex(-1.0f, 1.0f,-1.0f);
        prim.AddVertex(-1.0f, 1.0f, 1.0f);

        prim.SetColor(Color::Blue);
        prim.AddVertex( 1.0f,-1.0f, 1.0f);
        prim.AddVertex( 1.0f, 1.0f, 1.0f);
        prim.AddVertex(-1.0f, 1.0f, 1.0f);
        prim.AddVertex(-1.0f,-1.0f, 1.0f);

        prim.SetColor(Color::Cyan);
        prim.AddVertex(-1.0f, 1.0f, 1.0f);
        prim.AddVertex(-1.0f, 1.0f,-1.0f);
        prim.AddVertex(-1.0f,-1.0f,-1.0f);
        prim.AddVertex(-1.0f,-1.0f, 1.0f);

        prim.SetColor(Color::Purple);
        prim.AddVertex(-1.0f,-1.0f, 1.0f);
        prim.AddVertex(-1.0f,-1.0f,-1.0f);
        prim.AddVertex( 1.0f,-1.0f,-1.0f);
        prim.AddVertex( 1.0f,-1.0f, 1.0f);

        prim.SetColor(Color::Yellow);
        prim.AddVertex(-1.0f,-1.0f,-1.0f);
        prim.AddVertex(-1.0f, 1.0f,-1.0f);
        prim.AddVertex( 1.0f, 1.0f,-1.0f);
        prim.AddVertex( 1.0f,-1.0f,-1.0f);

        prim.EndDraw();
    }
}

namespace mlabs { namespace balai { namespace framework {

class MyApp : public BaseApp
{
    char         filename_[MAX_PATH];
    VoxelViewer  voxelViewer_;
    OrbitCamera  camera_;
    Transform    xform_;
    IAsciiFont*  font_;

    bool         verbose_;

    //
    int GetOpenFileName_(WCHAR* filename, int max_len) const {
        OPENFILENAME ofn;
        memset(&ofn, 0, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = WindowsHandle_();
        ofn.lpstrFile = filename; filename[0] = 0;
        ofn.nMaxFile = max_len;
        ofn.lpstrFilter = L"All\0*.*\0PVM\0*.PVM;*.pvm\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = L"./";
        ofn.lpstrTitle = L"Select PVM File...";
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
        if (GetOpenFileName(&ofn)) {
            return (int) wcslen(filename) + 1;
        }
        return 0;
    }
    int GetOpenFileName_(char* utf8, int max_len) const { // utf8 vresion
        WCHAR filename[MAX_PATH];
        int const wlen = GetOpenFileName_(filename, MAX_PATH);
        if (0<wlen) {
            int const len = WideCharToMultiByte(CP_UTF8, 0, filename, wlen, utf8, max_len, NULL, 0);
            if (0<len && len<max_len) {
                return len;
            }
        }
        return 0;
    }

public:
    MyApp():camera_(),xform_(),font_(NULL),
        verbose_(true) {
        memset(filename_, 0, sizeof(filename_));
        camera_.SetLookAt(math::Vector3(0.0f, -5.0f, 0.0f));
        camera_.SetPerspectiveParam(1.0f, 0.1f, 100.0f);
        window_title_ = "Volume Ray Marching demo by andre.hl.chen@gmail.com";
        width_ = 1280;
        height_ = 720;
    }
    bool Initialize() {
        if (voxelViewer_.Initialize()) {
            font_ = IAsciiFont::CreateFontFromFile("./assets/Gotham.font");
            voxelViewer_.LoadPVM("./assets/pvm/Foot.pvm", true);
            return true;
        }
        return false;
    }
    bool FrameMove(float frameTime) {
        return voxelViewer_.FrameUpdate(frameTime);
    }
    bool Render() {
        Renderer& renderer = Renderer::GetInstance();
        renderer.PushState();

        // world transform
        Matrix3 world = Matrix3::Identity;
        renderer.SetWorldMatrix(world);

        // view trasnform
        Matrix3 view(0.0f, 0.0f, 0.0f);
        Matrix3 camera;
        camera_.GetTransform().GetMatrix3(camera);
        //    view.SetEulerAngles(pitch_, roll_, yaw_)*Matrix3(0.0f, -dist_, 0.0f);
        renderer.SetViewMatrix(gfxBuildViewMatrixFromLTM(view, camera));
        view = camera.GetInverse(); // view is camera inverse

        // projection transform
        Matrix4 proj;
        gfxBuildPerspectiveProjMatrix(proj, 1.0f, renderer.GetFramebufferAspectRatio(), 0.1f, 100.0f);
        renderer.SetProjectionMatrix(proj);

        renderer.Clear(Color::Cyan);
        if (renderer.BeginScene(NULL)) {
            if (!voxelViewer_.Render(view*world)) {
                xform_.SetRotate(math::Quaternion(Vector3(0.6f, 0.0f, 0.8f), (float) GetAppTime()));
                xform_.GetMatrix3(world);
                renderer.SetWorldMatrix(world);
                DrawSomething();
            }

            if (font_) {
                if (verbose_) {
                    char msg[64];
                    std::sprintf(msg, "%.1f", GetFPS());
                    font_->DrawText(0.99f, 0.01f, 16, Color::Yellow, msg, FONT_ALIGN_RIGHT);
                }

                voxelViewer_.ShowInfo(font_, verbose_);
            }

            renderer.EndScene();
        }

        renderer.PopState();
        return true; 
    }
    void Cleanup() {
        voxelViewer_.Finalize();
        BL_SAFE_RELEASE(font_);
    }

    //void TouchBegan(uint32, uint32, float, float) {}
    //void TouchEnded(uint32, uint32, float, float) {}
    void TouchMoved(uint32 touch, uint32, float, float, float dx, float dy) {
        float dz(0.0f);
        if (0==touch) { // left mouse drag for rotating
            if (fabs(dx)>fabs(dy))
                camera_.Yaw(-3.0f*dx);
            else
                camera_.Pitch(-3.0f*dy);

            dx = dy = 0.0f;
        }
        else if (1==touch) { // middle mouse drag for zooming
            if (fabs(dx)>2.0f*fabs(dy)) {
                camera_.Roll(-3.0f*dx);
                dx = dy = 0.0f;
            }
            else {
                dx = 0.0f;
            }
        }
        else if (2==touch) {
            dz = dy;
            dy = 0.0f;
        }
        else {
            dx = dy = 0.0f;
        }

        // move
        if (0.0f!=dx || 0.0f!=dy || 0.0f!=dz) {
            float dist = camera_.GetTransform().Origin().Norm();
            if (dist<1.0f)
                dist = 1.0f;
            camera_.Move(-dx*dist, dy*dist, dz*dist*height_/width_);
        }
    }

    bool SDLEventHandler(SDL_Event const& event) {
        if (SDL_KEYDOWN==event.type) {
            SDL_KeyboardEvent const& key = event.key;
            switch (key.keysym.sym)
            {
            case SDLK_f: // Ctrl-F to trigger!
                if (SDL_PRESSED==key.state && 0==key.repeat &&
                    0!=(key.keysym.mod&(KMOD_LCTRL|KMOD_RCTRL))) {
                    if (GetOpenFileName_(filename_, sizeof(filename_)/sizeof(filename_[0]))) {
                        if (voxelViewer_.IsPVMFile(filename_)) {
                            camera_.SetLookAt(math::Vector3(0.0f, -5.0f, 0.0f));
                            if (voxelViewer_.LoadPVM(filename_, true)) {
                                return true;
                            }
                        }
                        else {
                            MessageBoxA(NULL, "Fail to load PVM File!", "Check File Format", 0);
                        }
                    }
                }
                break;

            case SDLK_v:
                verbose_ = !verbose_;
                break;

            case SDLK_F1:
                break;

            case SDLK_F2:
                break;

            case SDLK_F3:
                break;

            case SDLK_F4:
                break;

            case SDLK_F5:
                break;

            case SDLK_F10:
                break;

            case SDLK_F11:
                break;

            case SDLK_F12:
                break;

            case SDLK_KP_MINUS:
            case SDLK_MINUS:
                break;

            case SDLK_KP_PLUS:
            case SDLK_PLUS:
                break;

            case SDLK_UP:
                break;

            case SDLK_DOWN:
                break;

            case SDLK_LEFT:
                break;

            case SDLK_RIGHT:
                break;

            case SDLK_SPACE:
                // FIX-ME : a bug here...
                camera_.SetLookAt(camera_.GetTransform().Origin());
                return true;

            default:
                break;
            }
        }
        return false;
    }

    DECLARE_APPLICATION_CLASS;
};

IMPLEMENT_APPLICATION_CLASS(MyApp);

}}}