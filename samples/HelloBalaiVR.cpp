#include "BLApplication.h"
#include "BLCStdMemory.h"
#include "BLPrimitives.h"
#include "BLFont.h"

#include "BLVR.h"
#include "BLOpenGL.h"
#include "BLGLShader.h"

BL_CSTD_MEMMGR_IMPLMENTATION(2<<20, 1<<20);

using namespace mlabs::balai::graphics;
using namespace mlabs::balai::math;

void DrawLotsOfCubes(int counts)
{
    Renderer& renderer = Renderer::GetInstance();
    Primitives& prim = Primitives::GetInstance();

    // draw color faces cubes
    renderer.SetWorldMatrix(Matrix3::Identity);
    if (prim.BeginDraw(NULL, GFXPT_QUADLIST)) {
        Vector3 const box[6][4] = { // vertices
            {
                Vector3( 0.5f,-0.5f, 0.5f),
                Vector3( 0.5f,-0.5f,-0.5f),
                Vector3( 0.5f, 0.5f,-0.5f),
                Vector3( 0.5f, 0.5f, 0.5f),
            },

            {
                Vector3( 0.5f, 0.5f, 0.5f),
                Vector3( 0.5f, 0.5f,-0.5f),
                Vector3(-0.5f, 0.5f,-0.5f),
                Vector3(-0.5f, 0.5f, 0.5f),
            },

            {
                Vector3( 0.5f,-0.5f, 0.5f),
                Vector3( 0.5f, 0.5f, 0.5f),
                Vector3(-0.5f, 0.5f, 0.5f),
                Vector3(-0.5f,-0.5f, 0.5f),
            },

            {
                Vector3(-0.5f, 0.5f, 0.5f),
                Vector3(-0.5f, 0.5f,-0.5f),
                Vector3(-0.5f,-0.5f,-0.5f),
                Vector3(-0.5f,-0.5f, 0.5f),
            },

            {
                Vector3(-0.5f,-0.5f, 0.5f),
                Vector3(-0.5f,-0.5f,-0.5f),
                Vector3( 0.5f,-0.5f,-0.5f),
                Vector3( 0.5f,-0.5f, 0.5f),
            },

            {
                Vector3(-0.5f,-0.5f,-0.5f),
                Vector3(-0.5f, 0.5f,-0.5f),
                Vector3( 0.5f, 0.5f,-0.5f),
                Vector3( 0.5f,-0.5f,-0.5f),
            }
        };

        Color const face_colors[6] = {
            Color::Red,    // +X
            Color::Green,  // +Y
            Color::Blue,   // +Z
            Color::Cyan,   // -X
            Color::Purple, // -Y
            Color::Yellow  // -Z
        };

        Vector3 pos;
        float const spaces = 4.0f;
        float const offset = 0.5f*counts*spaces;
        for (int i=0; i<counts; ++i) {
            pos.x = i*spaces - offset;
            for (int j=0; j<counts; ++j) {
                pos.y = j*spaces - offset;
                for (int k=0; k<counts; ++k) {
                    pos.z = k*spaces - offset;
                    for (int f=0; f<6; ++f) {
                        prim.SetColor(face_colors[f]);
                        Vector3 const* box_face = box[f];
                        for (int v=0; v<4; ++v) {
                            prim.AddVertex(box_face[v]+pos);
                        }
                    }
                }
            }
        }
        prim.EndDraw();
    }
}

struct CubeField {
    ShaderEffect* fxColorOnly;
    GLuint        vao_, vbo_;
    int           num_vertices;

    CubeField():fxColorOnly(NULL),vao_(0),vbo_(0),num_vertices(0) {}
    bool Init() {
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        //glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        //glBufferData(GL_ARRAY_BUFFER, mesh.num_vertices*mesh.vertex_stride, mesh.vertices, GL_STATIC_DRAW);

        char const* vsh_n_c =
            "#version 410 core\n"
            "layout(location=0) in vec4 position;       \n"
            "layout(location=1) in mediump vec4 color;  \n"
            "uniform mat4 matWorldViewProj;             \n"
            "out vec4 color_;                           \n"
            "void main() {                              \n"
            "  gl_Position = position * matWorldViewProj; \n"
            "  color_ = color; \n"
            "}";
        char const* psh_c =
            "#version 410 core\n"
            "in lowp vec4 color_;\n"
            "layout(location=0) out vec4 outputColor;\n"
            "void main() { outputColor = color_; }";
        GLuint program = CreateGLProgram(vsh_n_c, psh_c);
        GLProgram* glShader = glShader = new GLProgram(0);
        if (glShader->Init(program)) {
            fxColorOnly = glShader;
        }
        else {
            BL_SAFE_RELEASE(glShader);
            return false;
        }
        return true;
    }
    void Destroy() {
        BL_SAFE_RELEASE(fxColorOnly);
        if (vbo_) {
            glDeleteBuffers(1, &vbo_);
            vbo_ = 0;
        }
        if (vao_) {
            glDeleteVertexArrays(1, &vao_);
            vao_ = 0;
        }
        num_vertices = 0;
    }

    void BuildVB(int sides) {
        num_vertices = sides*sides*sides*6*6;
        int const total_VB_size = num_vertices * 16;
        float* vertices = (float*) malloc(total_VB_size);

        Vector3 const box[6][4] = { // vertices
            {
                Vector3( 0.5f,-0.5f, 0.5f),
                Vector3( 0.5f,-0.5f,-0.5f),
                Vector3( 0.5f, 0.5f,-0.5f),
                Vector3( 0.5f, 0.5f, 0.5f),
            },

            {
                Vector3( 0.5f, 0.5f, 0.5f),
                Vector3( 0.5f, 0.5f,-0.5f),
                Vector3(-0.5f, 0.5f,-0.5f),
                Vector3(-0.5f, 0.5f, 0.5f),
            },

            {
                Vector3( 0.5f,-0.5f, 0.5f),
                Vector3( 0.5f, 0.5f, 0.5f),
                Vector3(-0.5f, 0.5f, 0.5f),
                Vector3(-0.5f,-0.5f, 0.5f),
            },

            {
                Vector3(-0.5f, 0.5f, 0.5f),
                Vector3(-0.5f, 0.5f,-0.5f),
                Vector3(-0.5f,-0.5f,-0.5f),
                Vector3(-0.5f,-0.5f, 0.5f),
            },

            {
                Vector3(-0.5f,-0.5f, 0.5f),
                Vector3(-0.5f,-0.5f,-0.5f),
                Vector3( 0.5f,-0.5f,-0.5f),
                Vector3( 0.5f,-0.5f, 0.5f),
            },

            {
                Vector3(-0.5f,-0.5f,-0.5f),
                Vector3(-0.5f, 0.5f,-0.5f),
                Vector3( 0.5f, 0.5f,-0.5f),
                Vector3( 0.5f,-0.5f,-0.5f),
            }
        };

        Color const face_colors[6] = {
            Color::Red,    // +X
            Color::Green,  // +Y
            Color::Blue,   // +Z
            Color::Cyan,   // -X
            Color::Purple, // -Y
            Color::Yellow  // -Z
        };

        float* ptr = vertices;
        Vector3 pos;
        float const spaces = 4.0f;
        float const offset = 0.5f*sides*spaces;
        for (int i=0; i<sides; ++i) {
            pos.x = i*spaces - offset;
            for (int j=0; j<sides; ++j) {
                pos.y = j*spaces - offset;
                for (int k=0; k<sides; ++k) {
                    pos.z = k*spaces - offset;
                    for (int f=0; f<6; ++f) {
                        Color const& color = face_colors[f];
                        Vector3 const* box_face = box[f];

                        *((Vector3*)ptr) = box_face[0]+pos; ptr+=3;
                        *((Color*)ptr) = color; ++ptr;
                        *((Vector3*)ptr) = box_face[1]+pos; ptr+=3;
                        *((Color*)ptr) = color; ++ptr;
                        *((Vector3*)ptr) = box_face[2]+pos; ptr+=3;
                        *((Color*)ptr) = color; ++ptr;

                        *((Vector3*)ptr) = box_face[0]+pos; ptr+=3;
                        *((Color*)ptr) = color; ++ptr;
                        *((Vector3*)ptr) = box_face[2]+pos; ptr+=3;
                        *((Color*)ptr) = color; ++ptr;
                        *((Vector3*)ptr) = box_face[3]+pos; ptr+=3;
                        *((Color*)ptr) = color; ++ptr;
                    }
                }
            }
        }

        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, total_VB_size, vertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 16, (GLvoid const*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 16, (GLvoid const*)12);

        free(vertices);
    }
    bool DrawSelf(int sides) {
        glBindVertexArray(vao_);
        int const total_cubes = sides*sides*sides;
        if (num_vertices!=total_cubes*12) {
            BuildVB(sides);
        }

        Renderer& renderer = Renderer::GetInstance();
        renderer.SetEffect(fxColorOnly);
        renderer.SetWorldMatrix(Matrix3::Identity);
        renderer.CommitChanges();
        glDrawArrays(GL_TRIANGLES, 0, num_vertices);
        glBindVertexArray(0);
        return true;
    }
};

namespace mlabs { namespace balai { namespace framework {

class MyApp : public BaseApp
{
    mlabs::balai::VR::Manager& vrMgr_;
    CubeField      world_;
    IAsciiFont*    font_;
    RenderSurface* surfaceL_;
    RenderSurface* surfaceR_;
    float          azimuth_;
    int            num_cube_array_;
    uint8          currMultisampleSamples_;
    uint8          nextMultisampleSamples_;
    uint8          maxMultisampleSamples_;
    uint8          vr_enable_;
    uint8          render_test_;

public:
    MyApp():vrMgr_(mlabs::balai::VR::Manager::GetInstance()),
        world_(),
        font_(NULL),
        surfaceL_(NULL),surfaceR_(NULL),
        azimuth_(0.0f),
        num_cube_array_(20),
        currMultisampleSamples_(0),nextMultisampleSamples_(0),maxMultisampleSamples_(1),
        vr_enable_(1),render_test_(0) {
        int width(0), height(0);
        if (vrMgr_.GetRenderSurfaceSize(width, height)) {
            width_ = (uint16) width;     // our app window size 
            height_ = (uint16) height/2;
            vsync_on_ = 0; // no vsync!
        }
        else {
            BL_ERR("VR system isn't ready!\n");
        }
    }
    bool BuildStereoRenderSurfaces(uint8 samples) {
        int width(0), height(0);
        if (vrMgr_.GetRenderSurfaceSize(width, height)) {
            SurfaceGenerator sgen;
            sgen.SetRenderTargetFormat(SURFACE_FORMAT_RGBA8);
            sgen.SetDepthFormat(SURFACE_FORMAT_D24);

            sgen.SetSurfaceSize(width, height);
            sgen.SetMultiSampleSamples(samples);

            BL_SAFE_RELEASE(surfaceL_);
            BL_SAFE_RELEASE(surfaceR_);

            surfaceL_ = sgen.Generate("Left View");
            surfaceR_ = sgen.Generate("Right View");
            return true;
        }
        return false;
    }
    bool Initialize() {
        maxMultisampleSamples_ = (uint8) Renderer::GetInstance().GetMaxMultisampleSamples();
        nextMultisampleSamples_ = 8;
        if (nextMultisampleSamples_>maxMultisampleSamples_)
            nextMultisampleSamples_ = maxMultisampleSamples_;
        maxMultisampleSamples_ += 1;
        if (!BuildStereoRenderSurfaces(currMultisampleSamples_=nextMultisampleSamples_)) {
            return false;
        }

        // first update pose
        vrMgr_.Initialize();
        vrMgr_.SetClippingPlanes(0.1f, 100.0f);
        vrMgr_.UpdatePoses();

        font_ = IAsciiFont::CreateFontFromFile("./assets/Gotham.font");

        return world_.Init();
    }
    bool FrameMove(float /*updateTime*/) {
        if (nextMultisampleSamples_!=currMultisampleSamples_) {
            if (BuildStereoRenderSurfaces(nextMultisampleSamples_)) {
                currMultisampleSamples_ = nextMultisampleSamples_;
            }
            else {
                return false;
            }
        }
        return true;
    }
    void DrawScene(VR::HMD_EYE eye) {
        assert(VR::HMD_EYE_LEFT==eye || VR::HMD_EYE_RIGHT==eye);
        Renderer& renderer = Renderer::GetInstance();
        renderer.SetSurface(VR::HMD_EYE_LEFT==eye ? surfaceL_:surfaceR_);

        Matrix3 mat(0.0f, 0.0f, 0.0f), pose;
        renderer.Clear(Color::Gray);
        if (renderer.BeginScene()) {
            vrMgr_.GetHMDPose(pose, eye);
            renderer.SetViewMatrix(gfxBuildViewMatrixFromLTM(mat, mat.SetEulerAngles(0.0f, 0.0f, azimuth_)*pose));
            renderer.SetProjectionMatrix(vrMgr_.GetProjectionMatrix(eye));
            //DrawLotsOfCubes(num_cube_array_);
            world_.DrawSelf(num_cube_array_);

            // draw 2 controllers
            Primitives& prim = Primitives::GetInstance();
            for (int i=0; i<2; ++i) {
                VR::TrackedDevice const* device = vrMgr_.GetTrackedDevice(i);
                if (device) {
                    device->DrawSelf();

                    // pointer
                    Vector3 from, dir;
                    device->GetPointer(from, dir);
                    if (device->IsTracked()) {
                        renderer.SetWorldMatrix(device->GetPose());
                        prim.BeginDraw(NULL, GFXPT_LINELIST);
                            prim.SetColor(Color::Red);
                            prim.AddVertex(0.0f, 0.0f, 0.0f);
                            prim.AddVertex(0.05f, 0.0f, 0.0f);
                            prim.SetColor(Color::Green);
                            prim.AddVertex(0.0f, 0.0f, 0.0f);
                            prim.AddVertex(0.0f, 0.05f, 0.0f);
                            prim.SetColor(Color::Blue);
                            prim.AddVertex(0.0f, 0.0f, 0.0f);
                            prim.AddVertex(0.0f, 0.0f, 0.05f);
                        prim.EndDraw();

                        // UI projects at 1~2m ?
                        renderer.SetWorldMatrix(Matrix3::Identity);
                        prim.BeginDraw(NULL, GFXPT_LINELIST);
                            prim.SetColor(Color::Yellow);
                            prim.AddVertex(from + 0.05f*dir);
                            prim.AddVertex(from += dir); // 1m
                            prim.SetColor(Color::Red);
                            prim.AddVertex(from);
                            prim.AddVertex(from += dir); // 2m
                            prim.SetColor(Color::Green);
                            prim.AddVertex(from);
                            prim.AddVertex(from += dir); // 3m
                            prim.SetColor(Color::Blue);
                            prim.AddVertex(from);
                            prim.AddVertex(from += dir); // 4m

                            prim.SetColor(Color::Red);
                            prim.AddVertex(from);
                            prim.AddVertex(from += dir); // 5m
                            prim.SetColor(Color::Green);
                            prim.AddVertex(from);
                            prim.AddVertex(from += dir); // 6m
                            prim.SetColor(Color::Blue);
                            prim.AddVertex(from);
                            prim.AddVertex(from += dir); // 7m

                            prim.SetColor(Color::Red);
                            prim.AddVertex(from);
                            prim.AddVertex(from += dir); // 8m
                            prim.SetColor(Color::Green);
                            prim.AddVertex(from);
                            prim.AddVertex(from += dir); // 9m
                            prim.SetColor(Color::Blue);
                            prim.AddVertex(from);
                            prim.AddVertex(from += dir); // 10m

                            prim.SetColor(Color::Purple);
                            prim.AddVertex(from);
                            prim.AddVertex(from + 100.0f*dir); // 10-100m

                        prim.EndDraw();
                    }
                }
            }
            renderer.EndScene();
        }
    }
    bool VRRenderPipelineTest() {
        static double pose_update_time_avg_ms = 0.0;
        static double pose_update_time = 0.0;
        static double submit_time_avg_ms = 0.0;
        static double submit_time = 0.0;
        static int frame_count = 0;
        static GLsync sync = 0;

        uint64 frame_id_start(0), frame_id(0);
        float vsync_time_start = 0.0f;
        //float update_pose_time = 0.0f;
        float vsync_time_poses_updated = 0.0f;
        int frame_offset_poses_updated = 0;

        // wait for poses
        if (vr_enable_) {
            vsync_time_start = 1000.0f*vrMgr_.GetTimeSinceLastVsync(frame_id_start);
            double time_start = system::GetTime();
            vrMgr_.UpdatePoses();   // stall until -3ms
            pose_update_time += 1000.0f*float(system::GetTime() - time_start);
            vsync_time_poses_updated = 1000.0f*vrMgr_.GetTimeSinceLastVsync(frame_id);
            frame_offset_poses_updated = (int) (frame_id - frame_id_start);
        }

        // draw scenes
        DrawScene(VR::HMD_EYE_LEFT);
        DrawScene(VR::HMD_EYE_RIGHT);

        // design flaw!?
        Renderer& renderer = Renderer::GetInstance();
        renderer.SetSurface(NULL);
        
        if (sync) {
            glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, 5000000);
            glDeleteSync(sync);
            sync = 0;
        }
        else {
            BL_LOG("sync = 0\n");
        }

        if (vr_enable_) {
            double time_start = system::GetTime();
            vrMgr_.Present(VR::HMD_EYE_LEFT, surfaceL_);
            vrMgr_.Present(VR::HMD_EYE_RIGHT, surfaceR_);
            submit_time += 1000.0f*float(system::GetTime() - time_start);
            if (++frame_count>=90) {
                pose_update_time_avg_ms = pose_update_time/frame_count;
                submit_time_avg_ms = submit_time/frame_count;
                frame_count = 0;
                submit_time = pose_update_time = 0.0;
            }
        }

        sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        GLReportErrors("glFenceSync");

        renderer.PushState();
        renderer.Clear(Color::Cyan);
        renderer.SetDepthMask(true);
        renderer.CommitChanges();
        if (renderer.BeginScene()) {
            Primitives& prim = Primitives::GetInstance();
            ITexture* tex = surfaceL_->GetRenderTarget(0);
            if (tex) {
                if (prim.BeginDraw(tex, GFXPT_SCREEN_QUADLIST)) {
                    prim.AddVertex2D(0.0f, 0.0f, 0.0f, 1.0f);
                    prim.AddVertex2D(0.0f, 1.0f, 0.0f, 0.0f);
                    prim.AddVertex2D(0.5f, 1.0f, 1.0f, 0.0f);
                    prim.AddVertex2D(0.5f, 0.0f, 1.0f, 1.0f);
                    prim.EndDraw();
                }
                tex->Release();
                tex = NULL;
            }

            tex = surfaceR_->GetRenderTarget(0);
            if (tex) {
                if (prim.BeginDraw(tex, GFXPT_SCREEN_QUADLIST)) {
                    prim.AddVertex2D(0.5f, 0.0f, 0.0f, 1.0f);
                    prim.AddVertex2D(0.5f, 1.0f, 0.0f, 0.0f);
                    prim.AddVertex2D(1.0f, 1.0f, 1.0f, 0.0f);
                    prim.AddVertex2D(1.0f, 0.0f, 1.0f, 1.0f);
                    prim.EndDraw();
                }
                tex->Release();
                tex = NULL;
            }

            // draw world coordinates
            Matrix3 pose;
            vrMgr_.GetHMDPose(pose);
            Vector3 const pos(pose._14, pose._24, pose._34); // position

            // world axes in pose frame
            Vector3 const xAxis(pose._11, pose._12, pose._13);
            Vector3 const yAxis(pose._21, pose._22, pose._23);
            Vector3 const zAxis(pose._31, pose._32, pose._33);

            // to be precise, it must be parallel projection.
            Matrix3 xform;
            float const w = 6.0f;
            float const h = 6.0f / renderer.GetFramebufferAspectRatio();
            Matrix4 parallelProj;

            renderer.SetWorldMatrix(Matrix3::Identity);
            renderer.PushViewMatrix(gfxBuildViewMatrixFromLTM(xform, Matrix3::Identity));
            renderer.PushProjMatrix(gfxBuildParallelProjMatrix(parallelProj, -w, w, -h, h, -2.0f, 2.0f));
            Vector3 const origin(-0.8f*w, 0.0f, -0.7f*h);
            prim.BeginDraw(NULL, GFXPT_LINELIST);
            prim.SetColor(Color::Red);
            prim.AddVertex(origin);
            prim.AddVertex(origin + xAxis);
            prim.SetColor(Color::Green);
            prim.AddVertex(origin);
            prim.AddVertex(origin + yAxis);
            prim.SetColor(Color::Blue);
            prim.AddVertex(origin);
            prim.AddVertex(origin + zAxis);
            prim.EndDraw();

            renderer.SetBlendMode(GFXBLEND_SRCALPHA, GFXBLEND_INVSRCALPHA);
            prim.BeginDraw(NULL, GFXPT_QUADLIST);
                Color const front_color = { 255, 255, 255, 192 };
                Color const back_color = { 128, 128, 128, 192 };
                prim.SetColor(front_color);
                Vector3 const v1 = 0.75f*(xAxis + yAxis);
                Vector3 const v2 = 0.75f*(xAxis - yAxis);
                prim.AddVertex(origin - v2);
                prim.AddVertex(origin - v1);
                prim.AddVertex(origin + v2);
                prim.AddVertex(origin + v1);

                prim.SetColor(back_color);
                prim.AddVertex(origin - v2);
                prim.AddVertex(origin + v1);
                prim.AddVertex(origin + v2);
                prim.AddVertex(origin - v1);
            prim.EndDraw();
            renderer.SetBlendMode(GFXBLEND_ONE, GFXBLEND_ZERO);
            renderer.PopViewMatrix();
            renderer.PopProjMatrix();
            renderer.SetDepthMask(false);

            if (font_) {
                char msg[64];
                float y = 0.01f;
                int const font_size = 24;
                Color const font_color = Color::Red;
                std::sprintf(msg, "FPS : %.1f", GetFPS());
                y += font_->DrawText(0.01f, y, font_size, font_color, msg) + 0.005f;
                std::sprintf(msg, "#(Cubes) : %d", num_cube_array_*num_cube_array_*num_cube_array_);
                y += font_->DrawText(0.01f, y, font_size, font_color, msg);

                if (currMultisampleSamples_>0) {
                    std::sprintf(msg, "MSAA : %dx", currMultisampleSamples_);
                    y += font_->DrawText(0.01f, y, font_size, font_color, msg);
                }
                else {
                    y += font_->DrawText(0.01f, y, font_size, Color::Red, "MSAA off");
                }

                if (vr_enable_) {/*
                    std::sprintf(msg, "HMD position :  %.3f  %.3f  %.3f m", pos.x, pos.y, pos.z);
                    y += font_->DrawText(0.01f, y, font_size, font_color, msg);
                    std::sprintf(msg, "X Axis :  %.3f  %.3f  %.3f", xAxis.x, xAxis.y, xAxis.z);
                    y += font_->DrawText(0.055f, y, font_size, font_color, msg);
                    std::sprintf(msg, "Y Axis :  %.3f  %.3f  %.3f", yAxis.x, yAxis.y, yAxis.z);
                    y += font_->DrawText(0.055f, y, font_size, font_color, msg);
                    std::sprintf(msg, "Z Axis :  %.3f  %.3f  %.3f", zAxis.x, zAxis.y, zAxis.z);
                    y += font_->DrawText(0.055f, y, font_size, font_color, msg); */
                    if (frame_offset_poses_updated>0) {
                        std::sprintf(msg, "Vsync start : -%d %.1f", -frame_offset_poses_updated, vsync_time_start);
                        y += font_->DrawText(0.01f, y, font_size, Color::Black, msg);
                    }
                    else {
                        std::sprintf(msg, "Vsync start : +%d %.1f", frame_offset_poses_updated, vsync_time_start);
                        y += font_->DrawText(0.01f, y, font_size, font_color, msg);
                    }
                    std::sprintf(msg, "Vsync waitpose : +0  %.1f", vsync_time_poses_updated);
                    y += font_->DrawText(0.01f, y, font_size, font_color, msg);

                    std::sprintf(msg, "Update Poses : %.1fms %.1f%%",
                                                pose_update_time_avg_ms,
                                                pose_update_time_avg_ms*GetFPS()/10.0f);
                    y += font_->DrawText(0.01f, y, font_size, font_color, msg);

                    std::sprintf(msg, "HMD Submit: %.1fms %.1f%%",
                                                submit_time_avg_ms,
                                                submit_time_avg_ms*GetFPS()/10.0f);
                    y += font_->DrawText(0.01f, y, font_size, font_color, msg);
                }
                else {
                    y += font_->DrawText(0.01f, y, font_size, Color::Yellow, "VR disable");
                }
            }

            renderer.EndScene();
        }
        renderer.PopState();

        return true;
    }

    bool Render() {
        if (render_test_) {
            return VRRenderPipelineTest();
        }

        // time metering(ugly!)
        static double pose_update_time_avg_ms = 0.0;
        static double pose_update_time = 0.0;
        static double submit_time_avg_ms = 0.0;
        static double submit_time = 0.0;
        static int frame_count = 0;

        // 1) submit last frames - this may take 0.x ms
        //    The submit images will be seen in HMD in the next vsync.
        //    You'll next call vrMgr_.UpdatePoses(); for waiting next HMD vsync,
        //    Instead of calling UpdatePoses, waiting for vsync and doing nothing,
        //    you should do some busy task like simulation and rendering and then
        //    call UpdatePoses(). in this way, you will wait less time, and have
        //    more time to do the fancy efects.
        //
        //    for more clever vsync manipulation, read...
        // http://media.steampowered.com/apps/valve/2015/Alex_Vlachos_Advanced_VR_Rendering_GDC2015.pdf
        //
        double time_begin = system::GetTime();
        if (vr_enable_) {
            vrMgr_.Present(VR::HMD_EYE_LEFT, surfaceL_);
            vrMgr_.Present(VR::HMD_EYE_RIGHT, surfaceR_);
        }
        submit_time += (system::GetTime() - time_begin);

        Renderer& renderer = Renderer::GetInstance();

        // design flaw...
        // must set back default renderbuffer before both present(surfaceR_)
        renderer.SetSurface(NULL);

        // 2) draw next frames to be presented in the next loop
        DrawScene(VR::HMD_EYE_LEFT);
        DrawScene(VR::HMD_EYE_RIGHT);

        renderer.PushState();

        renderer.SetSurface(NULL);
        renderer.Clear(Color::Cyan);
        renderer.SetDepthMask(true);
        renderer.CommitChanges();
        if (renderer.BeginScene()) {
            Primitives& prim = Primitives::GetInstance();
            ITexture* tex = surfaceL_->GetRenderTarget(0);
            if (tex) {
                if (prim.BeginDraw(tex, GFXPT_SCREEN_QUADLIST)) {
                    prim.AddVertex2D(0.0f, 0.0f,    0.0f, 1.0f);
                    prim.AddVertex2D(0.0f, 1.0f,    0.0f, 0.0f);
                    prim.AddVertex2D(0.5f, 1.0f,    1.0f, 0.0f);
                    prim.AddVertex2D(0.5f, 0.0f,    1.0f, 1.0f);
                    prim.EndDraw();
                }
                tex->Release();
                tex = NULL;
            }

            tex = surfaceR_->GetRenderTarget(0);
            if (tex) {
                if (prim.BeginDraw(tex, GFXPT_SCREEN_QUADLIST)) {
                    prim.AddVertex2D(0.5f, 0.0f,    0.0f, 1.0f);
                    prim.AddVertex2D(0.5f, 1.0f,    0.0f, 0.0f);
                    prim.AddVertex2D(1.0f, 1.0f,    1.0f, 0.0f);
                    prim.AddVertex2D(1.0f, 0.0f,    1.0f, 1.0f);
                    prim.EndDraw();
                }
                tex->Release();
                tex = NULL;
            }

            // draw world coordinates
            Matrix3 pose;
            vrMgr_.GetHMDPose(pose);
            Vector3 const pos(pose._14, pose._24, pose._34); // position

            // world axes in pose frame
            Vector3 const xAxis(pose._11, pose._12, pose._13);
            Vector3 const yAxis(pose._21, pose._22, pose._23);
            Vector3 const zAxis(pose._31, pose._32, pose._33);

            // to be precise, it must be parallel projection.
            Matrix3 xform;
            float const w = 6.0f;
            float const h = 6.0f / renderer.GetFramebufferAspectRatio();
            Matrix4 parallelProj;

            renderer.SetWorldMatrix(Matrix3::Identity);
            renderer.PushViewMatrix(gfxBuildViewMatrixFromLTM(xform, Matrix3::Identity));
            renderer.PushProjMatrix(gfxBuildParallelProjMatrix(parallelProj, -w, w, -h, h, -2.0f, 2.0f));
            Vector3 const origin(-0.8f*w, 0.0f, -0.7f*h);
            prim.BeginDraw(NULL, GFXPT_LINELIST);
            prim.SetColor(Color::Red);
            prim.AddVertex(origin);
            prim.AddVertex(origin + xAxis);
            prim.SetColor(Color::Green);
            prim.AddVertex(origin);
            prim.AddVertex(origin + yAxis);
            prim.SetColor(Color::Blue);
            prim.AddVertex(origin);
            prim.AddVertex(origin + zAxis);
            prim.EndDraw();

            renderer.SetBlendMode(GFXBLEND_SRCALPHA, GFXBLEND_INVSRCALPHA);
            prim.BeginDraw(NULL, GFXPT_QUADLIST);
                Color const front_color = { 255, 255, 255, 192 };
                Color const back_color = { 128, 128, 128, 192 };
                prim.SetColor(front_color);
                Vector3 const v1 = 0.75f*(xAxis + yAxis);
                Vector3 const v2 = 0.75f*(xAxis - yAxis);
                prim.AddVertex(origin - v2);
                prim.AddVertex(origin - v1);
                prim.AddVertex(origin + v2);
                prim.AddVertex(origin + v1);

                prim.SetColor(back_color);
                prim.AddVertex(origin - v2);
                prim.AddVertex(origin + v1);
                prim.AddVertex(origin + v2);
                prim.AddVertex(origin - v1);
            prim.EndDraw();
            renderer.SetBlendMode(GFXBLEND_ONE, GFXBLEND_ZERO);
            renderer.PopViewMatrix();
            renderer.PopProjMatrix();
            renderer.SetDepthMask(false);

            if (font_) {
                char msg[64];
                float y = 0.01f;
                int const font_size = 24;
                Color const font_color = Color::Black;
                std::sprintf(msg, "FPS : %.1f", GetFPS());
                y += font_->DrawText(0.01f, y, font_size, font_color, msg) + 0.005f;
                std::sprintf(msg, "#(Cubes) : %d", num_cube_array_*num_cube_array_*num_cube_array_);
                y += font_->DrawText(0.01f, y, font_size, font_color, msg);

                if (currMultisampleSamples_>0) {
                    std::sprintf(msg, "MSAA : %dx", currMultisampleSamples_);
                    y += font_->DrawText(0.01f, y, font_size, font_color, msg);
                }
                else {
                    y += font_->DrawText(0.01f, y, font_size, Color::Red, "MSAA off");
                }

                if (vr_enable_) { /*
                    std::sprintf(msg, "HMD position :  %.3f  %.3f  %.3f m", pos.x, pos.y, pos.z);
                    y += font_->DrawText(0.01f, y, font_size, font_color, msg);
                    std::sprintf(msg, "X Axis :  %.3f  %.3f  %.3f", xAxis.x, xAxis.y, xAxis.z);
                    y += font_->DrawText(0.055f, y, font_size, font_color, msg);
                    std::sprintf(msg, "Y Axis :  %.3f  %.3f  %.3f", yAxis.x, yAxis.y, yAxis.z);
                    y += font_->DrawText(0.055f, y, font_size, font_color, msg);
                    std::sprintf(msg, "Z Axis :  %.3f  %.3f  %.3f", zAxis.x, zAxis.y, zAxis.z);
                    y += font_->DrawText(0.055f, y, font_size, font_color, msg); */

                    std::sprintf(msg, "Update Poses : %.1fms %.1f%%",
                                                pose_update_time_avg_ms,
                                                pose_update_time_avg_ms*GetFPS()/10.0f);
                    y += font_->DrawText(0.01f, y, font_size, font_color, msg);

                    std::sprintf(msg, "HMD Submit: %.1fms %.1f%%",
                                                submit_time_avg_ms,
                                                submit_time_avg_ms*GetFPS()/10.0f);
                    y += font_->DrawText(0.01f, y, font_size, font_color, msg);
                }
                else {
                    y += font_->DrawText(0.01f, y, font_size, Color::Red, "VR disable");
                }
            }

            renderer.EndScene();
        }
        renderer.PopState();

        // predict next pose, wait for vsync
        if (vr_enable_) {
            time_begin = system::GetTime();
            vrMgr_.UpdatePoses();
            pose_update_time += (system::GetTime() - time_begin);
            if (++frame_count>=90) {
                pose_update_time_avg_ms = 1000.0 * pose_update_time/frame_count;
                submit_time_avg_ms = 1000.0 * submit_time/frame_count;
                frame_count = 0;
                submit_time = pose_update_time = 0.0;
            }
        }

        return true;
    }
    void Cleanup() {
        world_.Destroy();
        vrMgr_.Finalize();
        BL_SAFE_RELEASE(font_);
        BL_SAFE_RELEASE(surfaceL_);
        BL_SAFE_RELEASE(surfaceR_);
    }

    void TouchMoved(uint32 touch, uint32, float, float, float dx, float /*dy*/) {
        if (0==touch) { // left mouse drag for rotating
            azimuth_ += dx*2.0f;
        }
    }
    bool SDLEventHandler(SDL_Event const& event) {
        if (SDL_KEYDOWN==event.type) {
            SDL_KeyboardEvent const& key = event.key;
            switch (key.keysym.sym)
            {
            case SDLK_v:
                vr_enable_ = !vr_enable_;
                return true;

            case SDLK_SPACE:
                render_test_ = !render_test_;
                return true;

            case SDLK_KP_MINUS:
                if (currMultisampleSamples_>0) {
                    nextMultisampleSamples_ = currMultisampleSamples_ - 1;
                }
                else {
                    nextMultisampleSamples_ = maxMultisampleSamples_ - 1;
                }
                return true;

            case SDLK_KP_PLUS:
                nextMultisampleSamples_ = (currMultisampleSamples_ + 1)%maxMultisampleSamples_;
                return true;

            case SDLK_LEFT:
            case SDLK_UP:
                if (num_cube_array_>1)
                    --num_cube_array_;
                return true;

            case SDLK_RIGHT:
            case SDLK_DOWN:
                ++num_cube_array_;
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