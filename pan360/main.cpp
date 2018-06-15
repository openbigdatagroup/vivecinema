#include "BLApplication.h"
#include "BLCStdMemory.h"
#include "BLPrimitives.h"
#include "BLFont.h"

#include "BLFileStream.h"

#include "Pan360.h"

BL_CSTD_MEMMGR_IMPLMENTATION(32<<20, 1<<20);

using namespace mlabs::balai::graphics;
using namespace mlabs::balai::math;

namespace mlabs { namespace balai { namespace framework {

#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT  720

class Pan360Viewer : public BaseApp
{
    char           windowTitle_[128];
    htc::Pan360    viewer_;
    VR::Manager&   vrMgr_;
    IAsciiFont*    font_;
    RenderSurface* surfaceL_;
    RenderSurface* surfaceR_;
    RenderSurface* surfaceRef_;
    uint8          hmd_inited_;
    uint8          display_info_;
    uint8          fullmode_;
    uint8          reference_view_;

    uint16* LoadIconData_(int& width, int& height) {
        fileio::ifstream stream;
        if (stream.Open("./assets/asset.bin")) {
            fileio::FileHeader header;
            fileio::FileChunk chunk;
            stream >> header;
            if (0==memcmp(header.Description , "The Great Pan360", 16)) {
                do {
                    stream >> chunk;
                    if (chunk.ID==BL_MAKE_4CC('I', 'C', 'O', 'N')) {
                        int channels(0), dummy(0);
                        stream >> width >> height >> channels >> dummy;

                        int const pixel_size = width*height*2;
                        uint16* pixels = (uint16*) malloc(pixel_size);
                        stream.Read(pixels, pixel_size);
                        return pixels;
                    }
                    stream.SeekCur(chunk.ChunkSize());
                } while (!stream.Fail());
            }
        }
        return NULL;
    }
    void FreeIconData_(void* pixels, int, int) {
        free(pixels);
    }
    bool BuildStereoRenderSurfaces_(uint8 multisamplesamples) {
        int width(0), height(0);
        if (vrMgr_.GetRenderSurfaceSize(width, height)) {
            hmd_inited_ = 1;
        }
        else {
            ::MessageBoxA(NULL, "Sorry...\nOpenVR system isn't ready. No HMD!", window_title_, MB_OK);

            hmd_inited_ = 0;
            width  = 1512;
            height = 1680;
        }

        BL_SAFE_RELEASE(surfaceL_);
        BL_SAFE_RELEASE(surfaceR_);
        BL_SAFE_RELEASE(surfaceRef_);

        SurfaceGenerator sgen;
        sgen.SetRenderTargetFormat(SURFACE_FORMAT_RGBA8);
        sgen.SetDepthFormat(SURFACE_FORMAT_D24);
        sgen.SetSurfaceSize(width, height);
        sgen.SetMultiSampleSamples(multisamplesamples);
        surfaceL_ = sgen.Generate("Left View");
        surfaceR_ = sgen.Generate("Right View");
        surfaceRef_ = sgen.Generate("Reference View");
        return true;
    }
    void SetViewProjectionTransform_(Renderer& renderer, VR::HMD_EYE eye) const {
        if (hmd_inited_) {
            renderer.SetViewMatrix(vrMgr_.GetViewMatrix(eye));
            renderer.SetProjectionMatrix(vrMgr_.GetProjectionMatrix(eye));
        }
        else {
            // The interocular separation (or interpupulary distance) techinically refers to the distance
            // between the centers of the human eyes. This distance is typically accepted to be an average of 65mm
            // (roughly 2.5 inches) for a male adult
            float const interocular_separation = 0.065f;
            Matrix3 matViewL, matViewR;
            gfxBuildStereoViewMatricesFromLTM(matViewL, matViewR, Matrix3::Identity, interocular_separation);

            Matrix4 matProjL, matProjR;
            float const screen_plane_distance = 1.5f;
            gfxBuildStereoPerspectiveProjMatrices(matProjL, matProjR,
                                            110.0f*constants::float_deg_to_rad,
                                            (float)1512/(float)1680,
                                            interocular_separation,
                                            screen_plane_distance,
                                            0.1f, 100.0f);
            if (VR::HMD_EYE_LEFT==eye) {
                renderer.SetViewMatrix(matViewL);
                renderer.SetProjectionMatrix(matProjL);
            }
            else {
                renderer.SetViewMatrix(matViewR);
                 renderer.SetProjectionMatrix(matProjR);
            }
        }
    }
    bool DrawScene_(VR::HMD_EYE eye) {
        bool ok = false;
        Renderer& renderer = Renderer::GetInstance();
        renderer.SetSurface((VR::HMD_EYE_LEFT==eye) ? surfaceL_:surfaceR_);
        renderer.Clear(Color::Black);
        if (renderer.BeginScene()) {
            SetViewProjectionTransform_(renderer, eye);
            ok = viewer_.Render(eye);
            renderer.EndScene();
        }
        return ok;
    }

public:
    Pan360Viewer():viewer_(),vrMgr_(VR::Manager::GetInstance()),
        font_(NULL),surfaceL_(NULL),surfaceR_(NULL),surfaceRef_(NULL),
        hmd_inited_(0),display_info_(1),fullmode_(0),reference_view_(0) {
#if 1
        //
        // set current directory
        TCHAR module_path[512];
        int len = GetModuleFileNameW(NULL, module_path, 512);
        if (len>0) {
            for (int i=0; i<len; ++i) {
                if (module_path[i]==L'\\') {
                    module_path[i] = L'/';
                }
            }

            // strrchr
            while (len>=0 && module_path[len] != L'/') {
                module_path[len--] = L'\0';
            }

            if (len>0) {
                if (module_path[len]=='/')
                    module_path[len] = L'\0';

                SetCurrentDirectoryW(module_path);
            }
        }
#endif
        // window title
        strcpy(windowTitle_, "Pan360 - Equirectangular Map Viewer");
        int width(WINDOW_WIDTH), height(WINDOW_HEIGHT);
        viewer_.ReadConfig(L"./pan360.xml", windowTitle_, sizeof(windowTitle_), width, height);
        BaseApp::window_title_ = windowTitle_;
        BaseApp::vsync_on_  = 0; // do NOT waste anytime on main window vsync.
        BaseApp::fullscreen_ = (-1==width && -1==height) ? 1:0;

        int const screen_width  = GetSystemMetrics(SM_CXSCREEN);
        int const screen_height = GetSystemMetrics(SM_CYSCREEN);
        if (screen_width>=800 && screen_height>=600) {
            if (BaseApp::fullscreen_) {
                width = screen_width;
                height = screen_height;
            }
            else {
                if (width<0 || height<0) {
                    width  = WINDOW_WIDTH;
                    height = WINDOW_HEIGHT;
                }

                if (width>screen_width) {
                    height = width*height/screen_width;
                    width = screen_width;
                }

                if (height>screen_height)
                    height = screen_height;
            }
        }
        BaseApp::width_  = (uint16) width;
        BaseApp::height_ = (uint16) height;
    }

    bool Initialize() {
        uint32 const multisamplesamples = Renderer::GetInstance().GetMaxMultisampleSamples();
        BuildStereoRenderSurfaces_((multisamplesamples>8) ? 8:(uint8)multisamplesamples);

        // a font for debugging purpose
        font_ = IAsciiFont::CreateFontFromFile("./assets/Gotham.font");

        // init VR mgr and do first pose update
        if (hmd_inited_) {
            vrMgr_.Initialize();
            vrMgr_.SetClippingPlanes(0.1f, 100.0f);
            vrMgr_.UpdatePoses();
        }

        // init viewer
        if (!viewer_.Initialize()) {
            ::MessageBoxA(NULL, "Failed to Init Pan360 viewer!!! App must quit.", window_title_, MB_OK);
            return false;
        }

        return true;
    }
    bool FrameMove(float /*updateTime*/) {
        return true;
    }
    bool Render() {
        // present last result
        vrMgr_.Present(VR::HMD_EYE_LEFT, surfaceL_);
        vrMgr_.Present(VR::HMD_EYE_RIGHT, surfaceR_);

        // update video texture
        viewer_.FrameMove();

        // predict next pose, wait for vsync
        vrMgr_.UpdatePoses();

        Renderer& renderer = Renderer::GetInstance();

        // design flaw...
        // must set back default renderbuffer before both present(surfaceR_)
        renderer.SetSurface(NULL);

        // draw next frames to be presented in the next loop
        bool const ret = DrawScene_(VR::HMD_EYE_LEFT) && DrawScene_(VR::HMD_EYE_RIGHT);

        // draw reference view - use left-eye transforms to draw right-eye scene
        if (0==fullmode_ && 0!=reference_view_) {
            renderer.SetSurface(surfaceRef_);
            renderer.Clear(Color::Black);
            if (renderer.BeginScene()) {
                SetViewProjectionTransform_(renderer, VR::HMD_EYE_LEFT);
                viewer_.Render(VR::HMD_EYE_RIGHT);
                renderer.EndScene();
            }
        }

        renderer.PushState();

        renderer.SetSurface(NULL);
        renderer.Clear(Color::Black);
        renderer.SetDepthMask(true);
        renderer.CommitChanges();
        if (renderer.BeginScene()) {
            Primitives& prim = Primitives::GetInstance();
            float const dst_aspect_ratio = renderer.GetScreenAspectRatio();
            if (0==fullmode_) {
                ITexture* tex = (0==reference_view_) ?
                                surfaceL_->GetRenderTarget(0):surfaceRef_->GetRenderTarget(0);
                if (tex) {
                    if (prim.BeginDraw(tex, GFXPT_SCREEN_QUADLIST)) {
                        float const src_aspect_ratio = tex->Width()/(float)tex->Height();
                        float s0(0.0f), s1(1.0f), t0(0.0f), t1(1.0f);
                        if (src_aspect_ratio<dst_aspect_ratio) {
                            t1 = src_aspect_ratio/dst_aspect_ratio;
                            t0 = 0.5f - 0.5f*t1;
                            t1 += t0;
                        }
                        else if (src_aspect_ratio>dst_aspect_ratio) {
                            s1 = dst_aspect_ratio/src_aspect_ratio;
                            s0 = 0.5f - 0.5f*s1;
                            s1 += s0;
                        }
                        prim.AddVertex2D(0.0f, 0.0f, s0, t1);
                        prim.AddVertex2D(0.0f, 1.0f, s0, t0);
                        prim.AddVertex2D(1.0f, 1.0f, s1, t0);
                        prim.AddVertex2D(1.0f, 0.0f, s1, t1);
                        prim.EndDraw();
                    }
                    tex->Release();
                    tex = NULL;
                }
            }
            else {
                ITexture* tex = viewer_.GetEquirectangularMap();
                if (tex) {
                    float const src_aspect_ratio = tex->Width()/(float)tex->Height();
                    float x0(0.0f), x1(1.0f), y0(0.0f), y1(1.0f);
                    if (src_aspect_ratio<dst_aspect_ratio) {
                        x1 = src_aspect_ratio/dst_aspect_ratio;
                        x0 = 0.5f - 0.5f*x1;
                        x1 += x0;
                    }
                    else if (src_aspect_ratio>dst_aspect_ratio) {
                        y1 = dst_aspect_ratio/src_aspect_ratio;
                        y0 = 0.5f - 0.5f*y1;
                        y1 += y0;
                    }

                    if (prim.BeginDraw(tex, GFXPT_SCREEN_QUADLIST)) {
                        prim.AddVertex2D(x0, y0, 0.0f, 0.0f);
                        prim.AddVertex2D(x0, y1, 0.0f, 1.0f);
                        prim.AddVertex2D(x1, y1, 1.0f, 1.0f);
                        prim.AddVertex2D(x1, y0, 1.0f, 0.0f);
                        prim.EndDraw();
                    }
 
                    float const dw = (x1-x0)/tex->Width();
                    float const dh = (y1-y0)/tex->Height();
                    tex->Release();
                    tex = NULL;

                    if (prim.BeginDraw(NULL, GFXPT_SCREEN_LINESTRIP)) {
                        x0 += dw; x1 -= dw;
                        y0 += dh; y1 -= dh;
                        prim.SetColor(Color::Purple);
                        prim.AddVertex2D(x0, y0);
                        prim.AddVertex2D(x0, y1);
                        prim.AddVertex2D(x1, y1);
                        prim.AddVertex2D(x1, y0);
                        prim.AddVertex2D(x0, y0);
                        prim.EndDraw();
                    }
                }
            }

            // texts
            Color const vive_color = { 46, 161, 193, 255 };
            Color const info_color = Color::Yellow;
            Color const err_color = Color::Purple;
            int const font_size = 24;

            if (display_info_) {
                float const glyph_size = 1.8f*font_size/renderer.GetFramebufferHeight();
                viewer_.DrawInfo(vive_color, 0.005f, 0.0f, glyph_size);

                if (font_) {
                    char msg[256];

                    std::sprintf(msg, "%d", (int)(GetFPS()+0.5f));
                    font_->DrawText(0.99f, 0.99f, font_size, info_color, msg, FONT_ALIGN_RIGHT|FONT_ALIGN_BOTTOM);

                    if (!viewer_.IsLoading()) {
                        float y = 0.99f;
                        //y -= font_->DrawText(0.02f, y, font_size, info_color, "down : The last", FONT_ALIGN_BOTTOM);
                        //y -= font_->DrawText(0.02f, y, font_size, info_color, "up   : The first", FONT_ALIGN_BOTTOM);
                        y -= font_->DrawText(0.02f, y, font_size, info_color, "<< : prev     >> : next", FONT_ALIGN_BOTTOM);
                        y -= font_->DrawText(0.02f, y, font_size, info_color, "F3 : show entire image", FONT_ALIGN_BOTTOM);
                        y -= font_->DrawText(0.02f, y, font_size, info_color, "F2 : reference right-eye view", FONT_ALIGN_BOTTOM);
                        std::sprintf(msg, "F1  : 3D [%s]", viewer_.Stereoscopic3D());
                        y -= font_->DrawText(0.02f, y, font_size, info_color, msg, FONT_ALIGN_BOTTOM);

                        y -= 0.005f;
                        y -= font_->DrawText(0.01f, y, font_size+4, info_color, "Key Press -", FONT_ALIGN_BOTTOM);
                    }
                }
            }

            if (font_) {
                if (hmd_inited_) {
                    if (viewer_.IsLostTracking() && 0==fullmode_) {
                        font_->DrawText(0.5f, 0.5f, 36, err_color, "Lost Tracking ><", FONT_ALIGN_CENTER);
                    }
                }
                else {
                }
            }

            renderer.EndScene();
        }
        renderer.PopState();

        return ret;
    }
    void Cleanup() {
        viewer_.Finalize();
        vrMgr_.Finalize();
        BL_SAFE_RELEASE(font_);
        BL_SAFE_RELEASE(surfaceL_);
        BL_SAFE_RELEASE(surfaceR_);
        BL_SAFE_RELEASE(surfaceRef_);
    }
    bool SDLEventHandler(SDL_Event const& event) {
        if (SDL_WINDOWEVENT==event.type) {
            switch (event.window.event)
            {
            case SDL_WINDOWEVENT_FOCUS_GAINED:
                //BL_LOG("Window %d gained keyboard focus\n", event.window.windowID);
                //viewer_.ToggleImageChange(0);
                break;

            case SDL_WINDOWEVENT_FOCUS_LOST:
                //BL_LOG("Window %d lost keyboard focus\n", event.window.windowID);
                break;

            default:
                //BL_LOG("non-interested Window %d event %d\n", event.window.windowID, event.window.event);
                break;
            }
        }
        else if (SDL_KEYDOWN==event.type) {
            SDL_KeyboardEvent const& key = event.key;
            switch (key.keysym.sym)
            {
            case SDLK_d:
                display_info_ = (display_info_+1)%2;
                break;

            case SDLK_m:
                //multisamplesamples_ = 0xf0 | ???;
                break;

            case SDLK_F1:
                if (0==fullmode_)
                    viewer_.ToggleStereoscopic3D();
                return true;

            case SDLK_F2:
            case SDLK_SPACE:
            case SDLK_KP_SPACE:
                reference_view_ = 1;
                break;

            case SDLK_F3:
                fullmode_ = 1;
                break;

            case SDLK_LEFT:
                if (0==fullmode_)
                    viewer_.ToggleImageChange(-1);
                break;

            case SDLK_RIGHT:
                if (0==fullmode_)
                    viewer_.ToggleImageChange(1);
                break;

            case SDLK_UP:
                if (0==fullmode_)
                    viewer_.ToggleImageChange(0);
                break;

            case SDLK_DOWN:
                if (0==fullmode_)
                    viewer_.ToggleImageChange(0x12345678);
               break;

            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                return true;

            case SDLK_KP_MINUS:
            case SDLK_MINUS:
                break;

            case SDLK_KP_PLUS:
            case SDLK_PLUS:
                break;

            case SDLK_s:
            case SDLK_1:
                return true;

            default:
                break;
            }
        }
        else if (SDL_KEYUP==event.type) {
            SDL_KeyboardEvent const& key = event.key;
            switch (key.keysym.sym)
            {
            case SDLK_F2:
            case SDLK_SPACE:
            case SDLK_KP_SPACE:
                reference_view_ = 0;
                break;
            
            case SDLK_F3:
                fullmode_ = 0;
                break;

            default:
                break;
            }
        }
        return false;
    }

    // mouse move
    //void TouchBegan(uint32 touchId, uint32 tapCount, float x, float y) {}
    void TouchMoved(uint32 touchId, uint32 tapCount, float /*x*/, float /*y*/, float dx, float dy) {
        if (0==fullmode_ && 0==touchId && 0==tapCount) {
            if (fabs(dx)>2.0f*fabs(dy)) {
                viewer_.AdjustAzimuthAngle(dx);
            }
            else if (2.0f*fabs(dx)<fabs(dy)) {
                //viewer_.AdjustPitchAngle(dy); to be implemented
            }
        }
    }
    //void TouchEnded(uint32 touchId, uint32 tapCount, float x, float y) {}

    DECLARE_APPLICATION_CLASS;
};

IMPLEMENT_APPLICATION_CLASS(Pan360Viewer);

}}}