#include "BLApplication.h"
#include "BLCStdMemory.h"
#include "BLPrimitives.h"
#include "BLFont.h"

#include "BLSphericalHarmonics.h"

//
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// !!! TO-DO : verify FuMa rotation style !!!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//

BL_CSTD_MEMMGR_IMPLMENTATION(64<<20, 1<<20);

using namespace mlabs::balai::graphics;
using namespace mlabs::balai::math;

namespace mlabs { namespace balai { namespace framework {

struct TestSHShape {
    double operator()(Sphere2 const& s) const {
        double const a = 5.0*std::cos(s.Theta) - 4.0;
        double const b = -4.0 * sin(s.Theta-constants::double_pi)*cos(s.Phi - 2.5) - 3.0;
        return (BL_MAX(0.0, a) + BL_MAX(0.0, b));
    }
};

int const ambix2fuma_ch[16] = {
    0,
    3, 1, 2,
    6, 7, 5, 8, 4,
    12, 13, 11, 14, 10, 15, 9,
};

double const ambix2fuma_scale[16] = { // ambiX to FuMa (no reordering)
    0.70710678118654752440084436210485,
    1.0f, 1.0f, 1.0f,
    1.1547005383792515290182975610039, // V
    1.1547005383792515290182975610039, // T
    1.0f, // R
    1.1547005383792515290182975610039, // S
    1.1547005383792515290182975610039, // U
    1.2649110640673517327995574177731,
    1.2649110640673517327995574177731,
    1.1858541225631422494995850791623,
    1.0f,
    1.1858541225631422494995850791623,
    1.2649110640673517327995574177731,
    1.2649110640673517327995574177731,
};

class MyApp : public BaseApp
{
    //enum { TEST_BANDS = SphericalHarmonics::MAX_BANDS };
    enum { MAX_TEST_BANDS = 8 };
    double sh_delta_[MAX_TEST_BANDS*MAX_TEST_BANDS];

    // font
    IAsciiFont* font_;

    // an eval SH shapre
    SphericalHarmonics::SHCoeffs sh_init_;

    // rotation
    float distance_, yaw_, pitch_, roll_;
    float objyaw_, objpitch_, objroll_;

    // test 
    int test_Ylm_;
    int rotation_style_; // 0:SH, 1:ambiX
    int test_bands_;
    int tweak_lm_;

    bool condon_shortley_, ctrlDown_, altDown_;

public:
    MyApp():
        font_(NULL),
        sh_init_(),
        distance_(2.0f),yaw_(0.0f),pitch_(0.0f),roll_(0.0f),
        objyaw_(0.0f),objpitch_(0.0f),objroll_(0.0f),
        test_Ylm_(MAX_TEST_BANDS*MAX_TEST_BANDS),
        rotation_style_(1),test_bands_(MAX_TEST_BANDS),tweak_lm_(MAX_TEST_BANDS*MAX_TEST_BANDS),
        condon_shortley_(false),ctrlDown_(false),altDown_(false) {
        memset(sh_delta_, 0, sizeof(sh_delta_));
        window_title_ = "Spherical Harmonics";
        //BaseApp::fullscreen_ = 1;
        width_ = 1440;
        height_ = 720;
    }
    ~MyApp() {}

    bool Initialize() {
        font_ = IAsciiFont::CreateFontFromFile("./assets/Gotham.font"); // font

        // build a shape
        int const TEST_SAMPLES = 500;
        Sphere2 spherical_coord[TEST_SAMPLES];
        UniformDistributeSamplesOverSphere(spherical_coord, TEST_SAMPLES);
        SphericalHarmonics::Project(sh_init_, MAX_TEST_BANDS, TestSHShape(), spherical_coord, TEST_SAMPLES, condon_shortley_);

        return true;
    }

    bool FrameMove(float /*updateTime*/) { return true; }

    bool DrawSH(int l, int m) const {
        int const NUM_SEGMENTS = 50;
	    // Steps along the sphere
        double du = math::constants::double_pi/NUM_SEGMENTS;
	    double dv = 2.0*math::constants::double_pi/NUM_SEGMENTS;
        Primitives& prim = Primitives::GetInstance();

        if (prim.BeginDraw(NULL, GFXPT_QUADLIST)) {
            double const eval_max = 1.25f*sqrt(3.0/(4.0*math::constants::double_pi));
            double eval, t;
            Color red(Color::Red), green(Color::Green);
            for (double u=0.0; u<math::constants::double_pi; u+=du) {
		        for (double v=0.0; v<2.0*math::constants::double_pi; v+=dv) {
                    double uv[4][2] = {
                        { u, v },
                        { (u + du), v },
                        { (u + du), (v + dv) },
                        { u, (v + dv) }
			        };
                    
                    for (int k=0; k<4; ++k) {
                        eval = SphericalHarmonics::EvalSH(l, m, uv[k][0], uv[k][1], condon_shortley_);
                        if (eval<0.0) {
                            eval = -eval;
                            t = eval/eval_max;
                            if (t>0.99f) {
                                red.r = 255;
                            }
                            else {
                                t = 255.0*t*t*(3.0-2.0*t);
                                if (t<255.0f)
                                    red.r = (uint8)t;
                                else
                                    red.r = 255;
                            }
                            prim.SetColor(red);
                        }
                        else {
                            t = eval/eval_max;
                            if (t>0.99f) {
                                green.g = 255;
                            }
                            else {
                                t = 255.0*t*t*(3.0-2.0*t);
                                if (t<255.0f)
                                    green.g = (uint8)t;
                                else
                                    green.g = 255;
                            }
                            prim.SetColor(green);
                        }
                        prim.AddVertex((float)eval*Sphere2((float)uv[k][0], (float)uv[k][1]).Cartesian());
                    }
                }
            }

            prim.EndDraw();
            return true;
        }

        return false;
    }
    bool DrawSH(SphericalHarmonics::SHCoeffs const& sh) const {
        int const NUM_SEGMENTS = 50;

	    // Steps along the sphere
        double du = math::constants::double_pi/NUM_SEGMENTS;
	    double dv = 2.0*math::constants::double_pi/NUM_SEGMENTS;
        Primitives& prim = Primitives::GetInstance();

        if (prim.BeginDraw(NULL, GFXPT_QUADLIST)) {
            double const eval_max = 1.25f*sqrt(3.0/(4.0*math::constants::double_pi));
            double eval, t;
            Color red(Color::Red), green(Color::Green);
            for (double u=0.0; u<math::constants::double_pi; u+=du) {
		        for (double v=0.0; v<2.0*math::constants::double_pi; v+=dv) {
                    double uv[4][2] = {
                        { u, v },
                        { (u + du), v },
                        { (u + du), (v + dv) },
                        { u, (v + dv) }
			        };
                    
                    for (int k=0; k<4; ++k) {
                        eval = sh.EvalSH(uv[k][0], uv[k][1], condon_shortley_);
                        if (eval<0.0) {
                            eval = -eval;
                            t = eval/eval_max;
                            if (t>0.99f) {
                                red.r = 255;
                            }
                            else {
                                t = 255.0*t*t*(3.0-2.0*t);
                                if (t<255.0f)
                                    red.r = (uint8)t;
                                else
                                    red.r = 255;
                            }
                            prim.SetColor(red);
                        }
                        else {
                            t = eval/eval_max;
                            if (t>0.99f) {
                                green.g = 255;
                            }
                            else {
                                t = 255.0*t*t*(3.0-2.0*t);
                                if (t<255.0f)
                                    green.g = (uint8)t;
                                else
                                    green.g = 255;
                            }
                            prim.SetColor(green);
                        }
                        prim.AddVertex((float)eval*Sphere2((float)uv[k][0], (float)uv[k][1]).Cartesian());
                    }
                }
            }

            prim.EndDraw();
            return true;
        }

        return false;
    }

    bool Render() {
        static int scounter = 0;
        static double stime = 0.0;
        static double sh_rotate_time = 0.0;
        Matrix3 obj(0.0f, 0.0f, 0.0f);
        obj.SetEulerAngles(objpitch_, objroll_, objyaw_);

        SphericalHarmonics::SHCoeffs sh;
        SphericalHarmonics::SHCoeffs sh_rotate;
        double Ylm[MAX_TEST_BANDS*MAX_TEST_BANDS];
        if (test_Ylm_<test_bands_*test_bands_) {
            memset(Ylm, 0, sizeof(Ylm));
            Ylm[test_Ylm_] = 1.0;
            sh.Set(test_bands_, Ylm);
        }
        else {
            if (test_bands_==sh_init_.NumBands()) {
                sh = sh_init_;
            }
        }

        for (int i=0; i<test_bands_*test_bands_; ++i) {
            sh[i] += sh_delta_[i];
        }

        double const t0 = system::GetTime();

        SphericalHarmonics::SHRotateMatrix shRot;
        if (1==rotation_style_) {
            shRot.BuildRotationMatrixAmbiX(test_bands_, obj);
        }
        else {
            shRot.BuildRotationMatrix(test_bands_, obj, condon_shortley_);
        }

        //
        //
        // TO-DO : scale and remap channels if FuMa
        //
        //

        float ylm[MAX_TEST_BANDS*MAX_TEST_BANDS];
        for (int i=0; i<test_bands_*test_bands_; ++i) {
            ylm[i] = (float) sh[i];
        }
        shRot.TransformSafe(ylm, ylm, test_bands_*test_bands_);
        for (int i=0; i<test_bands_*test_bands_; ++i) {
            Ylm[i] = ylm[i];
        }
        sh_rotate.Set(test_bands_, Ylm);

        stime += system::GetTime() - t0;
        if (++scounter>100) {
            sh_rotate_time = stime/scounter;
            stime = 0.0;
            scounter = 0;
        }

        Renderer& renderer = Renderer::GetInstance();
        renderer.Clear(Color::Black);
        renderer.PushState();

        // world as identity
        renderer.SetWorldMatrix(Matrix3::Identity);

        // modify view
        Matrix3 mat(0.0f, 0.0f, 0.0f), viewLtm;
        viewLtm = mat.SetEulerAngles(pitch_, roll_, yaw_)*Matrix3(0.0f, -distance_, 0.0f);
        renderer.SetViewMatrix(gfxBuildViewMatrixFromLTM(mat, viewLtm));

        // projection
        Matrix4 proj;
        gfxBuildPerspectiveProjMatrix(proj, 1.0f, renderer.GetFramebufferAspectRatio(), 0.1f, 100.0f);
        renderer.SetProjectionMatrix(proj);

        if (renderer.BeginScene()) {
            Primitives& prim = Primitives::GetInstance();

            if (test_Ylm_) {
                renderer.SetZTestDisable();
                if (prim.BeginDraw(NULL, GFXPT_LINELIST)) {
                    prim.SetColor(Color::White);
                    for (float i=-10.0f; i<=10.0f; ++i) {
                        prim.AddVertex(i, -10.0f, 0.0f);
                        prim.AddVertex(i, 10.0f, 0.0f);
                
                        prim.AddVertex(-10.0f, i, 0.0f);
                        prim.AddVertex(10.0f, i, 0.0f);
                    }

                    prim.SetColor(Color::Red);
                    prim.AddVertex(0.0f, 0.0f, 0.0f);
                    prim.AddVertex(10.0f, 0.0f, 0.0f);

                    prim.SetColor(Color::Green);
                    prim.AddVertex(0.0f, 0.0f, 0.0f);
                    prim.AddVertex(0.0f, 10.0f, 0.0f);

                    prim.SetColor(Color::Blue);
                    prim.AddVertex(0.0f, 0.0f, 0.0f);
                    prim.AddVertex(0.0f, 0.0f, 10.0f);

                    prim.EndDraw();
                }

                renderer.SetZTest();
                renderer.SetCullDisable();

                Matrix3 ltm(obj);
                if (rotation_style_) {
                    if (1==rotation_style_) {
                        ltm._11= obj._22; ltm._12=-obj._21; ltm._13= obj._23;
                        ltm._21=-obj._12; ltm._22= obj._11; ltm._23=-obj._13;
                        ltm._31= obj._32; ltm._32=-obj._31; ltm._33= obj._33;
                    }
                    else {
                        ltm._11= obj._22; ltm._12=-obj._21; ltm._13= obj._23;
                        ltm._21=-obj._12; ltm._22= obj._11; ltm._23=-obj._13;
                        ltm._31= obj._32; ltm._32=-obj._31; ltm._33= obj._33;
                    }
                }

                if (!altDown_) {
                    renderer.SetWorldMatrix(Matrix3(2.0f, 0.0, 0.0f)*ltm);
                }
                else {
                    renderer.SetWorldMatrix(ltm);
                }
                DrawSH(sh);
                renderer.SetWorldMatrix(Matrix3::Identity);
                DrawSH(sh_rotate);
            }
            else {
                float const h[] = { 4.5f, 3.64f, 2.45f, 1.0f, -0.65f, -2.5f, -4.5f,
                                             -6.0f, -7.5f, -9.0f, -12.0f, -15.0f, -18.0f };
                float const w[] = { 0.0f, 1.0f, 2.1f, 3.25f, 4.5f, 5.75f, 7.1f,
                                             -8.5f, -9.9f, -11.4f, -13.0f, -15.0f, -17.0f };
                for (int l=0; l<test_bands_; ++l) {
                    for (int m=-l; m<=l; ++m) {
                        renderer.SetWorldMatrix(Matrix3(m<0 ? -w[-m]:w[m], 0.0f, h[l])*obj);
                        DrawSH(l, m);
                    }
                }
            }

            renderer.SetCullMode(GFXCULL_BACK);
         
            char msg[256];
            float y = 0.01f;
            sprintf(msg, "FPS : %.1f", GetFPS());

            if (rotation_style_) {
                y += font_->DrawText(0.01f, y, 20, Color::White, "Rotation Style [R] : ambiX");
                y += font_->DrawText(0.01f, y, 20, Color::Gray, "Condon-Shortley phase : Off");
            }
            else {
                y += font_->DrawText(0.01f, y, 20, Color::White, "Rotation Style [R] : SH");
                sprintf(msg, "Condon-Shortley phase [C] : %s", condon_shortley_ ? "On":"Off");
                y += font_->DrawText(0.01f, y, 20, Color::White, msg);
            }

            //y += font_->DrawText(0.01f, y, 20, Color::White, msg);
            if (0<test_Ylm_) {
                //sprintf(msg, "SH rotate time:%.4fms (%d bands)", 1000.0f*sh_rotate_time, test_bands_);
                //y += font_->DrawText(0.01f, y, 20, Color::White, msg);
                if (test_Ylm_<test_bands_*test_bands_) {
                    //sprintf(msg, "SH(%d) function", test_Ylm_);
                    //y += font_->DrawText(0.01f, y, 20, Color::White, msg);
                }
                if (!altDown_) {
                    y += 0.05f;
                    int tweak_l = 0;
                    int tweak_m = 0;
                    for (int i=0; i<test_bands_; ++i) {
                        float const offset0 = 0.052f;
                        float const offset1 = 0.0053f;
                        float x0 = 0.01f;
                        float x1 = 1.0f - x0 - offset0;
                        for (int j=-i; j<=i; ++j) {
                            double s = sh(i, j);
                            if (-0.00005<s && s<0.00005f) { // don't want to print out -0.0000
                                s = 0.0000;
                            }
                            sprintf(msg, "%.4f", s);
                            font_->DrawText(s>0 ? x0 : (x0 - offset1), y, 12, (s != 0) ? Color::White : Color::Gray, msg);

                            s = sh_rotate(i, -j);
                            if (-0.00005<s && s<0.00005f) {
                                s = 0.0000;
                            }
                            sprintf(msg, "%.4f", s);
                            float const dy = font_->DrawText(s > 0 ? x1 : (x1 - offset1), y + 0.45f, 12, (s != 0) ? Color::Yellow : Color::Gray, msg);
                            
                            x0 += offset0;
                            x1 -= offset0;

                            if (j == i)
                                y += dy;

                            if (tweak_lm_ == (i*(i + 1) + j)) {
                                tweak_l = i;
                                tweak_m = j;
                            }
                        }
                    }

                    if (tweak_lm_<(MAX_TEST_BANDS*MAX_TEST_BANDS)) {
                        sprintf(msg, "Tweak Y(%d, %d)  [Enter/+/-]", tweak_l, tweak_m);
                        font_->DrawText(0.5f, 0.995f, 24, Color::Cyan, msg, FONT_ALIGN_CENTERED_X|FONT_ALIGN_BOTTOM);
                    }
                    else {
                        font_->DrawText(0.5f, 0.995f, 24, Color::Cyan, "Tweak mode [Enter]", FONT_ALIGN_CENTERED_X|FONT_ALIGN_BOTTOM);
                    }
                }
            }
            else {
                sprintf(msg, "Spherical Harmonics first %d bands", test_bands_);
                y += font_->DrawText(0.01f, y, 20, Color::White, msg);
            }

            renderer.EndScene();
        }

        renderer.PopState();
        return true; 
    }
    void Cleanup() {
        BL_SAFE_RELEASE(font_);
    }

    void TouchBegan(uint32 touch, uint32, float, float) {
        if (1==touch) {
            objyaw_ = objroll_ = objpitch_ = 0.0f;
        }
    }
    void TouchMoved(uint32 touch, uint32, float, float, float dx, float dy) {
        if (0==touch) { // left mouse drag for rotating
            if (!ctrlDown_) {
                pitch_ = math::Clamp(pitch_-4.0f*dy, -1.5f, 1.5f); // pi/2 ~= 1.570796
                yaw_  = fmod(yaw_-4.0f*dx, constants::float_two_pi);
            }
            else {
                objpitch_ = math::Clamp(objpitch_-4.0f*dy, -1.5f, 1.5f);
                objyaw_  = fmod(objyaw_-4.0f*dx, constants::float_two_pi);
            }
        }
        else if (1==touch) { // middle mouse drag for zooming
            distance_ -= 6.0f*distance_*dy;
            if (distance_<0.01f) {
                distance_ = 0.01f;
            }
            else if (distance_>100.0f) {
                distance_ = 100.0f;
            }
        }
        else if (2==touch) {
            if (!ctrlDown_) {
                roll_ = math::Clamp(roll_-4.0f*dx, -1.5f, 1.5f); // pi/2 ~= 1.570796
            }
            else {
                objroll_ = math::Clamp(objroll_-4.0f*dx, -1.5f, 1.5f);
            }
        }
    }
    //void TouchEnded(uint32, uint32, float, float) {}
    bool SDLEventHandler(SDL_Event const& event) {
        if (SDL_KEYDOWN==event.type) {
            SDL_KeyboardEvent const& key = event.key;
            switch (key.keysym.sym)
            {
            case SDLK_s:
                return true;

            case SDLK_c:
                if (rotation_style_) {
                    condon_shortley_ = false;
                }
                else {
                    condon_shortley_ = !condon_shortley_;
                }
                return true;

            case SDLK_r:
                rotation_style_ = (rotation_style_+1)%2;
                if (rotation_style_) {
                    condon_shortley_ = false;
                }
                return true;

            case SDLK_LEFT:
                return true;

            case SDLK_RIGHT:
                return true;

            case SDLK_KP_PLUS:
                if (tweak_lm_<(MAX_TEST_BANDS*MAX_TEST_BANDS)) {
                    sh_delta_[tweak_lm_] += 0.1f;
                }
                break;

            case SDLK_KP_MINUS:
                if (tweak_lm_<(MAX_TEST_BANDS*MAX_TEST_BANDS)) {
                    sh_delta_[tweak_lm_] -= 0.1f;
                }
                break;

            case SDLK_KP_ENTER:
            case SDLK_RETURN:
                if (tweak_lm_<(MAX_TEST_BANDS*MAX_TEST_BANDS)) {
                    tweak_lm_ = (tweak_lm_+1)%(MAX_TEST_BANDS*MAX_TEST_BANDS);
                }
                else {
                    tweak_lm_ = 0;
                }
                break;

            case SDLK_DOWN:
                break;

            case SDLK_UP:
                objpitch_ = objyaw_  = objroll_ = 0.0f;
                break;

            case SDLK_1:
            case SDLK_2:
            case SDLK_3:
            case SDLK_4:
                tweak_lm_ = (MAX_TEST_BANDS*MAX_TEST_BANDS);
                memset(sh_delta_, 0, sizeof(sh_delta_));
                test_Ylm_ = test_bands_*test_bands_;
                objpitch_ = objyaw_ = objroll_ = 0.0f;
                break;

            case SDLK_SPACE:
                tweak_lm_ = (MAX_TEST_BANDS*MAX_TEST_BANDS);
                memset(sh_delta_, 0, sizeof(sh_delta_));
                test_Ylm_ = (test_Ylm_+1)%(test_bands_*test_bands_+1);
                objpitch_ = objyaw_  = objroll_ = 0.0f;
                return true;

            case SDLK_LCTRL:
                ctrlDown_ = true;
                return true;

            case SDLK_LALT:
                altDown_ = true;
                return true;

            default:
                break;
            }
        }
        else if (SDL_KEYUP==event.type) {
            SDL_KeyboardEvent const& key = event.key;
            if (SDLK_LCTRL==key.keysym.sym) {
                ctrlDown_ = false;
                return true;
            }
            else if (SDLK_LALT==key.keysym.sym) {
                altDown_ = false;
                return true;
            }
        }
        return false;
    }

    DECLARE_APPLICATION_CLASS;
};

IMPLEMENT_APPLICATION_CLASS(MyApp);

}}}