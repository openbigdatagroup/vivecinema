#include "BLApplication.h"
#include "BLCStdMemory.h"
#include "BLPrimitives.h"
#include "BLFont.h"
#include "BLCamera.h"

BL_CSTD_MEMMGR_IMPLMENTATION(2<<20, 1<<20);

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
    Camera      camera_;
    Transform   xform_;
    IAsciiFont* font_;
    ITexture*   tex_;
    uint32      onTouches_;
    uint32      touchCount_;

public:
    MyApp():camera_(),xform_(),font_(NULL),tex_(NULL),
        onTouches_(0),touchCount_(0) {
        camera_.SetLookAt(math::Vector3(0.0f,-5.0f,0.0f));
        //camera_.SetNearFarPlanes(1.0f, 7.0f);
        window_title_ = "Hello World!";
    }
    bool Initialize() {
        font_ = IAsciiFont::CreateFontFromFile("./assets/Gotham.font");

        //
        // this is a counter-example. it reminds users to meet
        // 4 bytes(default) alignment requirements for the start of each pixel row in memory.
        Texture2D* tex2D = Texture2D::New(0);
        int const width = 126; // !!! 126*3 is not in 4 bytes alignment, make it 128 to fix this !!!
        int const height = 128;
        uint8* pixels = new uint8[width*height*3];

        // make texture:
        //  Red  | Green
        //  Blue | Gray
        uint8 const color[4][3] = {
            { 0xff, 0x00, 0x00 }, // Red
            { 0x00, 0xff, 0x00 }, // Green
            { 0x00, 0x00, 0xff }, // Blue
            { 0x80, 0x80, 0x80 }, // White
        };

        uint8* dst = pixels;
        for (int i=0; i<height; ++i) {
            for (int j=0; j<width; ++j) {
                memcpy(dst, color[2*(i>(height/2)) + (j>(width/2))], 3);
                dst += 3;
            }
        }
        tex2D->UpdateImage((uint16)width, (uint16)height, FORMAT_RGB8, pixels);
        tex_ = tex2D;

        //
        // pixels are uploaded to GPU, you can delete pixels (in client memory).
        // or leak report is coming up.(check debug output when app closed)
        delete[] pixels;

        onTouches_ = touchCount_ = 0;

        return true;
    }
    bool FrameMove(float updateTime) {
        static float angle = 0.0f;
        angle += updateTime;
        xform_.SetRotate(math::Quaternion(Vector3(0.6f, 0.0f, 0.8f), angle));
        return true;
    }
    bool Render() {
        Renderer& renderer = Renderer::GetInstance();
        renderer.PushState();
        Matrix3 matWorld;
        renderer.SetWorldMatrix(xform_.GetMatrix3(matWorld));
        renderer.Clear(Color::Black);
        if (renderer.BeginScene(&camera_)) {
            if (2<=onTouches_) { // to trigger : press mouse 2 buttons
                Primitives& prim = Primitives::GetInstance();
                if (prim.BeginDraw(tex_, GFXPT_SCREEN_QUADLIST)) {
                    prim.SetColor(Color::White);
                    prim.AddVertex2D(0.25f, 0.25f, 0.0f, 0.0f); // let-top corner
                    prim.AddVertex2D(0.25f, 0.75f, 0.0f, 1.0f); // left-bottom
                    prim.AddVertex2D(0.75f, 0.75f, 1.0f, 1.0f); // right-bottom
                    prim.AddVertex2D(0.75f, 0.25f, 1.0f, 0.0f); // right-top
                    prim.EndDraw();
                }
            }
            else {
                DrawSomething();
            }
            if (font_) {
                char msg[64];
                float y = 0.01f;
                std::sprintf(msg, "fps = %.2f", GetFPS());
                y += font_->DrawText(0.01f, y, 24, Color::Yellow, msg);
                std::sprintf(msg, "on touch = %d", onTouches_);
                y += font_->DrawText(0.01f, y, 24, Color::Yellow, msg);
                std::sprintf(msg, "touch counter = %d", touchCount_);
                y += font_->DrawText(0.01f, y, 24, Color::Yellow, msg);

                if (1==onTouches_) {
                    font_->DrawText(0.5f, 0.5f, 72, Color::Purple, "I love balai 3D", FONT_ALIGN_CENTER);
                }
                else {
                    font_->DrawText(0.5f, 0.5f, 72, Color::White, "Hello World!", FONT_ALIGN_CENTER);
                }
            }
            renderer.EndScene();
        }
        renderer.PopState();
        return true; 
    }
    void Cleanup() {
        BL_SAFE_RELEASE(font_);
        BL_SAFE_RELEASE(tex_);
    }
    void TouchBegan(uint32, uint32, float, float) {
        ++onTouches_;
        ++touchCount_;
    }
    void TouchEnded(uint32, uint32, float, float) {
        --onTouches_;
    }
    //void TouchMoved(uint32, uint32, float, float, float dx, float dy) {
    //yaw_ += dx;
    //pitch_ += dy;
    //}

    DECLARE_APPLICATION_CLASS;
};

IMPLEMENT_APPLICATION_CLASS(MyApp);

}}}