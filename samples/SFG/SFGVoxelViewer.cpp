/* 
 * MIT license
 *
 * Copyright (c) 2017 SFG(Star-Formation Group), Physics Dept., CUHK.
 * All rights reserved.
 *
 * file    SFGVoxelViewer.cpp
 * author  andre chen, andre.hl.chen@gmail.com
 * history 2017/10/27 created
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
#include "SFGVoxelViewer.h"

#include "BLGraphics.h"
#include "BLPrimitives.h"
#include "BLVector3.h"

using namespace mlabs::balai::graphics;
using namespace mlabs::balai::math;

namespace sfg {

//---------------------------------------------------------------------------------------
void VoxelViewer::ReadVoxels_(uint8 const* voxels,
                              int cx, int cy, int cz, int components,
                              float sx, float sy, float sz,
                              char const* description,
                              char const* courtesy,
                              char const* parameters,
                              char const* comment)
{
    if (description) {
        BL_LOG("Description : %s\n", description);
        if (courtesy) {
            if ('\0'!=*courtesy) {
                BL_LOG("Courtesy : %s\n", courtesy);
            }

            if (parameters) {
                if ('\0'!=*parameters) {
                    BL_LOG("Parameters : %s\n", parameters);
                }

                if (comment && '\0'!=*comment) {
                    BL_LOG("Comment : %s\n", comment);
                }
            }
        }
    }

    if (voxels && cx>0 && cy>0 && cz>0 && components>0) {
#if 0
        {
            char filename[256];
            int const pixels = cx*cy;
            uint8* image = (uint8*) malloc(pixels);
            uint8 const* src = voxels + components - 1;
            for (int i=0; i<cz; ++i) {
                uint8* dst = image;
                for (int j=0; j<pixels; ++j,src+=components) {
                    *dst++ = src[0];
                }
                sprintf(filename, "./data/test_pvm/img%03d.jpg", i+1);
                mlabs::balai::image::write_JPEG(filename, image, cx, cy, 1);
            }
            free(image);
        }
#endif
        float* vox = (float*) malloc(cx*cy*cz*sizeof(float));
        if (PVMParser::ReadVoxels_(vox, voxInf_, voxSup_,
                                   voxels, cx, cy, cz, components)) {
            sizeX_ = cx; // width
            sizeY_ = cy; // height
            sizeZ_ = cz; // depth

            scaleX_ = 1.0f; // check!>
            scaleY_ = sy/sx;
            scaleZ_ = sz/sx;

            // what unit? m, cm, mm?
            //float const pixel_size = sx;         // m
            //float const pixel_size = sx/100.0f;  // cm
            //float const pixel_size = sx/1000.0f; // mm
            float const pixel_size = 0.005f; // 5mm
            supX_ = scaleX_*0.5f*pixel_size*(sizeX_-1);
            supY_ = scaleY_*0.5f*pixel_size*(sizeY_-1);
            supZ_ = scaleZ_*0.5f*pixel_size*(sizeZ_-1);
            infX_ = -supX_;
            infY_ = -supY_;
            infZ_ = -supZ_;

            int const sizeX = BL_ALIGN_UP(sizeX_, 4); // width
            int const sizeY = BL_ALIGN_UP(sizeY_, 4); // depth
            int const sizeZ = BL_ALIGN_UP(sizeZ_, 4); // height
            size_t const sizeXY  = sizeX*sizeY;
            size_t const sizeXYZ = sizeXY*sizeZ;
            size_t const size_pad_X = sizeX - sizeX_;
            size_t const size_pad_Y = (sizeY - sizeY_)*sizeX;
            size_t const size_pad_Z = (sizeZ - sizeZ_)*sizeXY;

            memset(histogram_, 0, sizeof(histogram_));
            uint8* pixel = voxels_ = (uint8*) malloc(sizeXYZ*sizeof(uint8));
            if (NULL==voxels_) {
                free(vox);
                if (voxels_) {
                    free(voxels_);
                    voxels_ = NULL;
                }
                strcpy(message_, "failed to process FITS array data");
                status_= STATUS_LOADING_FAILED;
                return;
            }

            memset(voxels_, 0, sizeXYZ*sizeof(uint8));
            float const scale = 255.0f/(voxSup_-voxInf_);
            uint8* p = voxels_;
            int v;
            for (int z=0; STATUS_LOADING==status_&&z<sizeZ_; ++z) {
                progressing_ = 100*z/sizeZ_;
                float const* src = vox + (sizeZ_-z-1)*sizeX_*sizeY_;
                for (int y=0; y<sizeY_; ++y) {
                    for (int x=0; x<sizeX_; ++x,++p,++src) {
                        v = (int) (((*src)-voxInf_)*scale);
                        if (0<v) {
                            if (v<256) {
                                ++histogram_[*p=v];
                            }
                            else {
                                ++histogram_[*p=255];
                            }
                        }
                    }
                    p += size_pad_X;
                }
                p += size_pad_Y;
            }
            p += size_pad_Z;

            // align up
            sizeX_ = sizeX;
            sizeZ_ = sizeZ;
            sizeY_ = sizeY;
        }

        free(vox);
    }
}
//---------------------------------------------------------------------------------------
void VoxelViewer::LoadPVM_()
{
    assert(STATUS_LOADING == status_);
    if (STATUS_LOADING == status_) {
        if (voxels_) {
            free(voxels_);
            voxels_ = NULL;
        }
        sizeX_ = sizeY_ = sizeZ_ = 0;
        infX_ = infY_ = infZ_ = 0.0f;
        supX_ = supY_ = supZ_ = 0.0f;
        scaleX_ = scaleY_ = scaleZ_ = 1.0f;

        strcpy(message_, "loading PVM file...");
        if (PVMParser::Load(fullName_)) {
            status_ = STATUS_LOADED;
        }
        else {
            status_ = STATUS_LOADING_FAILED;
        }
    }
}
//---------------------------------------------------------------------------------------
bool VoxelViewer::Initialize()
{
    // vertex shader
    char const* vp = "#version 410 core\n"
        "layout(location=0) in vec2 pos;\n"
        "out vec2 tc;\n"
        "void main() {\n"
        "  gl_Position = vec4(2.0*pos-1.0, -1.0, 1.0);	\n"
        "  tc = pos;\n"
        "}";

    char const* fp = "#version 410 core\n"
        "uniform vec4 step;\n"
        "uniform mediump sampler2D ray_begin;\n"
        "uniform mediump sampler2D ray_end;\n"
        "uniform sampler3D voxel;\n"
        "in vec2 tc;\n"
        "layout(location=0) out vec4 c0;\n"
        "void main() {\n"
        "  vec3 s = texture(ray_begin, tc).xyz;\n"
        "  vec3 dir = texture(ray_end, tc).xyz - s;\n"
        "  float dist = length(dir);\n"
        "  vec4 dst = vec4(0, 0, 0, 0);\n"
        "  if (dist>0.0) {\n"
        "    vec4 d = vec4(step.x*dir/dist, step.x);\n"
        "    vec4 t = vec4(s, 0.0);\n"
        "    vec4 s = vec4(1.0, 1.0, 1.0, 0.0);\n"
        "    while (t.w<dist && dst.a<0.95) {\n"
        "      s = texture(voxel, t.xyz).xxxx;\n"
        "      if (s.a>0.25) {\n"
        "        s.a *= step.w;\n"
        "        s.rgb *= s.a;\n"
        "        dst = (1.0f - dst.a)*s + dst;\n"
        "      }\n"
        "      t += d;\n"
        "    }\n"
        "  } else {\n"
        "    discard;\n"
        "  }\n"
        "  c0 = dst;\n"
        "}";

    uint32 program = CreateGLProgram(vp, fp);
    GLProgram* glShader = new GLProgram(0);
    if (glShader->Init(program)) {
        rayMarching_ = glShader;
        step_ = glShader->FindConstant("step");
        ray_start_ = glShader->FindSampler("ray_begin");
        ray_end_ = glShader->FindSampler("ray_end");
        voxel_ = glShader->FindSampler("voxel");
    }
    else {
        return false;
    }

    char const* vp2 = "#version 410 core\n"
        "uniform mat4 matWorldViewProj;\n"
        "uniform vec4 matTC[3];\n"
        "uniform vec4 clipping;\n"
        "layout(location=0) in vec4 pos;\n"
        "layout(location=1) in vec3 txyz;\n"
        "out vec3 t3;\n"
        "void main() {\n"
        "  gl_Position = (pos*clipping)*matWorldViewProj;\n"
        "  vec4 tc = vec4(txyz.x*clipping.x, txyz.y, txyz.z*clipping.z, 1.0);\n"
        "  t3.x = dot(matTC[0], tc);\n"
        "  t3.y = dot(matTC[1], tc);\n"
        "  t3.z = dot(matTC[2], tc);\n"
        "}";
    char const* fp2 = "#version 410 core\n"
        "in vec3 t3;\n"
        "layout(location=0) out vec4 c0;\n"
        "void main() {\n"
        "  if (t3.x<0.0 || t3.y<0.0 || t3.z<0.0 || t3.x>1.0 || t3.y>1.0 || t3.z>1.0) discard;\n"
        "  c0.xyz = t3;\n"
        "  c0.w = 1.0;\n"
        "}";

    program = CreateGLProgram(vp2, fp2);
    glShader = new GLProgram(0);
    if (glShader->Init(program)) {
        tcFx_     = glShader;
        matTC_    = glShader->FindConstant("matTC");
        clipping_ = glShader->FindConstant("clipping");
    }
    else {
        return false;
    }

    uint32 const width = Renderer::GetInstance().GetScreenWidth();
    uint32 const height = Renderer::GetInstance().GetScreenHeight();
    if (width>0 && height>0) {
        SurfaceGenerator sgen;
        sgen.SetRenderTargetFormat(SURFACE_FORMAT_RGBA16F);
        sgen.SetDepthFormat(SURFACE_FORMAT_D24);
        sgen.SetSurfaceSize(width, height);
        sgen.SetMultiSampleSamples(1);
        sgen.SetDepthStencilUsage(SurfaceGenerator::DEPTH_STENCIL_USAGE_DEFAULT_READONLY);

        BL_SAFE_RELEASE(frontCap_);
        BL_SAFE_RELEASE(backCap_);

        frontCap_ = sgen.Generate("frontcap");
        backCap_ = sgen.Generate("backcap");
    }

    // volumeMap
    volumeMap_ = Texture3D::New(0);

    return true;
}
//---------------------------------------------------------------------------------------
void VoxelViewer::Finalize()
{
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if (vao_) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }

    if (vbo_) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }

    BL_SAFE_RELEASE(tcFx_);
    BL_SAFE_RELEASE(rayMarching_);
    step_ = matTC_  = clipping_ = NULL;
    ray_start_ = ray_end_ = voxel_ = NULL;

    BL_SAFE_RELEASE(frontCap_);
    BL_SAFE_RELEASE(backCap_);

    BL_SAFE_RELEASE(volumeMap_);

    status_ = STATUS_EMPTY;
    if (thread_.joinable()) {
        thread_.join();
    }

    status_ = STATUS_EMPTY;
}
//---------------------------------------------------------------------------------------
bool VoxelViewer::FrameUpdate(float frameTime)
{
    if (STATUS_LOADING==status_) {
        // nothing?
    }
    else if (STATUS_LOADED==status_) {
        if (thread_.joinable()) {
            thread_.join();
        }

        volumeMap_->UpdateImage(sizeX_, sizeY_, sizeZ_, FORMAT_I8, voxels_);

        // vao_ and vbo_
        if (0==vao_) {
            glGenVertexArrays(1, &vao_);
            if (0==vao_) {
                strcpy(message_, "Fail to glGenVertexArrays()");
                BL_LOG("%s\n", message_);
                return true;
            }
        }

        if (0==vbo_) {
            glGenBuffers(1, &vbo_);
            if (0==vbo_) {
                strcpy(message_, "Fail to glGenBuffers()");
                BL_LOG("%s\n", message_);
                return true;
            }
        }

        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);

        float const rx = 0.5f*(supX_ - infX_);
        float const ry = 0.5f*(supY_ - infY_);
        float const rz = 0.5f*(supZ_ - infZ_);
        float const rr = mlabs::balai::math::Sqrt(rx*rx + ry*ry + rz*rz);
        float const dy = 2.0f*ry/(float)(sizeY_-1); // slice distance
        slices_ = (int) (2.0f*rr/dy); // total slices

        // vertex buffer (x, y, z) for position and (s, t, r) for 3D texture texcoord
        int const vertex_stride = 6*sizeof(float);
        int const vertex_bufer_size = (2*slices_-1)*4*vertex_stride;
        float* vertex_buffer = (float*) malloc(vertex_bufer_size);
        float* vb = vertex_buffer;

        // extended volume texture coordinate, set origin at box center
        float const tc = 0.5f*rr/rx;
        float const dt = 2.0f*tc/((float)slices_-1.0f);

        // arrange quad stack from back to front
        float y0 = rr;
        float t0 = tc; // t-coord step
        for (int i=0; i<slices_; ++i,y0-=dy,t0-=dt) {
            *vb++ = -rr; // left-top corner
            *vb++ =  y0;
            *vb++ =  rr;
            *vb++ = -tc;
            *vb++ =  t0;
            *vb++ =  tc;

            *vb++ = -rr; // left-bottom corner
            *vb++ =  y0;
            *vb++ = -rr;
            *vb++ = -tc;
            *vb++ =  t0;
            *vb++ = -tc;

            *vb++ =  rr; // right-bottom corner
            *vb++ =  y0;
            *vb++ = -rr;
            *vb++ =  tc;
            *vb++ =  t0;
            *vb++ = -tc;

            *vb++ =  rr; // right-top corner
            *vb++ =  y0;
            *vb++ =  rr;
            *vb++ =  tc;
            *vb++ =  t0;
            *vb++ =  tc;
        }

        for (int i=1; i<slices_; ++i) {
            memcpy(vb, vertex_buffer + 24*(slices_-1-i), 24*sizeof(float));
            vb += 24;
        }

        glBufferData(GL_ARRAY_BUFFER, vertex_bufer_size, vertex_buffer, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vertex_stride, 0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vertex_stride, (GLvoid const*)12);
        glBindVertexArray(0);

        free(vertex_buffer);

        sprintf(message_, "%s | [%.4f, %.4f]x[%.4f, %.4f]x[%.4f, %.4f] | %dx%dx%d",
                shortName_, infX_, supX_, infZ_, supZ_, infY_, supY_, sizeX_, sizeZ_, sizeY_);
        BL_LOG("loading completed : time = %.3f secs\n", mlabs::balai::system::GetTime());

        status_ = STATUS_READY;
    }
    else if (STATUS_VOLUMEMAP_UPDATE==status_) {
        volumeMap_->UpdateImage(sizeX_, sizeY_, sizeZ_, FORMAT_I8, voxels_);
        status_ = STATUS_READY;
    }
    else if (STATUS_READY==status_) {
        //
        // simulation here!?
        //
    }

    return true;
}
//---------------------------------------------------------------------------------------
bool VoxelViewer::Render(Matrix3 const& matModelView)
{
    if (slices_<=0 || STATUS_READY!=status_)
        return false;

    // draw background
    Renderer& renderer = Renderer::GetInstance();
    renderer.PushState();

    // primitive
    Primitives& prim = Primitives::GetInstance();

    // volume size
    float const rx = 0.5f*(supX_ - infX_);
    float const ry = 0.5f*(supY_ - infY_);
    float const rz = 0.5f*(supZ_ - infZ_);

    // culling and clipping slices
    mlabs::balai::math::Vector3 const verts[8] = {
        matModelView.VectorTransform(mlabs::balai::math::Vector3(-rx, -ry, -rz)),
        matModelView.VectorTransform(mlabs::balai::math::Vector3(-rx, -ry,  rz)),
        matModelView.VectorTransform(mlabs::balai::math::Vector3(-rx,  ry, -rz)),
        matModelView.VectorTransform(mlabs::balai::math::Vector3(-rx,  ry,  rz)),
        matModelView.VectorTransform(mlabs::balai::math::Vector3( rx, -ry, -rz)),
        matModelView.VectorTransform(mlabs::balai::math::Vector3( rx, -ry,  rz)),
        matModelView.VectorTransform(mlabs::balai::math::Vector3( rx,  ry, -rz)),
        matModelView.VectorTransform(mlabs::balai::math::Vector3( rx,  ry,  rz)),
    };
    mlabs::balai::math::Vector3 inf(verts[0]), sup(verts[0]);
    for (int i=1; i<8; ++i) {
        mlabs::balai::math::Vector3 const& v = verts[i];
        if (v.x<inf.x)
            inf.x = v.x;
        else if (v.x>sup.x)
            sup.x = v.x;

        if (v.y<inf.y)
            inf.y = v.y;
        else if (v.y>sup.y)
            sup.y = v.y;

        if (v.z<inf.z)
            inf.z = v.z;
        else if (v.z>sup.z)
            sup.z = v.z;
    }
    float const rr = mlabs::balai::math::Sqrt(rx*rx + ry*ry + rz*rz);
    int const slice_start = (int)(slices_*(1.0f - sup.y/rr))/2;
    int const slice_totals = slices_ - 2*slice_start; // by symmetric

#if 0
    // write depth buffer with farmost
    renderer.SetCullMode(GFXCULL_FRONT);
    renderer.SetColorWrite(false);
    renderer.SetDepthWrite(true);
    if (prim.BeginDraw(NULL, GFXPT_QUADLIST)) {
        prim.AddVertex( rx, -ry,  rz);
        prim.AddVertex( rx, -ry, -rz);
        prim.AddVertex( rx,  ry, -rz);
        prim.AddVertex( rx,  ry,  rz);

        prim.AddVertex( rx,  ry,  rz);
        prim.AddVertex( rx,  ry, -rz);
        prim.AddVertex(-rx,  ry, -rz);
        prim.AddVertex(-rx,  ry,  rz);

        prim.AddVertex( rx, -ry,  rz);
        prim.AddVertex( rx,  ry,  rz);
        prim.AddVertex(-rx,  ry,  rz);
        prim.AddVertex(-rx, -ry,  rz);

        prim.AddVertex(-rx,  ry,  rz);
        prim.AddVertex(-rx,  ry, -rz);
        prim.AddVertex(-rx, -ry, -rz);
        prim.AddVertex(-rx, -ry,  rz);

        prim.AddVertex(-rx, -ry,  rz);
        prim.AddVertex(-rx, -ry, -rz);
        prim.AddVertex( rx, -ry, -rz);
        prim.AddVertex( rx, -ry,  rz);

        prim.AddVertex(-rx, -ry, -rz);
        prim.AddVertex(-rx,  ry, -rz);
        prim.AddVertex( rx,  ry, -rz);
        prim.AddVertex( rx, -ry, -rz);

        prim.EndDraw();
    }

    // actually, we're drawing all slices far to near
    renderer.SetColorWrite(true);
    renderer.SetZTest(GFXCMP_LESSEQUAL);
#endif

    //renderer.SetZTestDisable();
    renderer.SetCullDisable(); // cull must disable

    // world
    Matrix3 mat(matModelView._14, matModelView._24, matModelView._34);
    renderer.PushWorldMatrix(mat);

    // view as identity
    renderer.PushViewMatrix(gfxBuildViewMatrixFromLTM(mat, Matrix3::Identity));

    // texcoord transform matrix
    mat = matModelView.GetInverse();
    float const sS = 1.0f/scaleX_;
    float const sT = ((float)sizeX_)/(scaleY_*sizeY_);
    float const sR = ((float)sizeX_)/(scaleZ_*sizeZ_);
    mat._11 *= sS; mat._12 *= sS; mat._13 *= sS;
    mat._21 *= sT; mat._22 *= sT; mat._23 *= sT;
    mat._31 *= sR; mat._32 *= sR; mat._33 *= sR;
    mat.SetOrigin(0.5f, 0.5f, 0.5f);
    float const clipping[4] = { sup.x/rr, 1.0f, sup.z/rr, 1.0f };


    renderer.SetBlendDisable();
    renderer.SetZTest(GFXCMP_LESSEQUAL);
    renderer.SetDepthWrite(false);

    renderer.SetSurface(frontCap_);
    renderer.Clear(Color::Black, 1.0f, 0, GFX_CLEAR_COLOR);
    renderer.SetEffect(tcFx_);
    tcFx_->BindConstant(matTC_, &mat);
    tcFx_->BindConstant(clipping_, clipping);
    renderer.CommitChanges();
    glBindVertexArray(vao_);
    glDrawArrays(GL_QUADS, 4*slice_start, 4*slice_totals);

    renderer.SetSurface(backCap_);
    renderer.Clear(Color::Black, 1.0f, 0, GFX_CLEAR_COLOR);
    renderer.SetEffect(tcFx_);
    tcFx_->BindConstant(matTC_, &mat);
    tcFx_->BindConstant(clipping_, clipping);
    renderer.CommitChanges();
    glDrawArrays(GL_QUADS, 4*(slices_+slice_start), 4*slice_totals);

    glBindVertexArray(0);
    renderer.PopWorldMatrix();
    renderer.PopViewMatrix();

    // deferred shading...
    renderer.SetSurface(NULL);
    renderer.SetBlendMode(GFXBLEND_SRCALPHA, GFXBLEND_INVSRCALPHA);
    ITexture* end = backCap_->GetRenderTarget(0);
    if (end) {
        ITexture* start = frontCap_->GetRenderTarget(0);
        if (start) {
            renderer.SetEffect(rayMarching_);
            float const step[4] = { 1.0f/sizeX_, 1.0f/sizeY_, 1.0f/sizeZ_, 0.025f };
            rayMarching_->BindConstant(step_, step);
            rayMarching_->BindSampler(ray_start_, start);
            rayMarching_->BindSampler(ray_end_, end);
            rayMarching_->BindSampler(voxel_, volumeMap_);
            renderer.CommitChanges();
            uint8 const vb_[] = { 0,255,  0,0,  255,255,  255,0 }; // fullscreen quad
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_UNSIGNED_BYTE, GL_TRUE, 2, vb_);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

#define DEBUG_SHOW_CAP_RENDER_BUFFER
#ifdef DEBUG_SHOW_CAP_RENDER_BUFFER
            if (prim.BeginDraw(start, GFXPT_SCREEN_QUADLIST)) {
                prim.AddVertex2D(0.005f, 0.585f, 0.0f, 1.0f);
                prim.AddVertex2D(0.005f, 0.785f, 0.0f, 0.0f);
                prim.AddVertex2D(0.205f, 0.785f, 1.0f, 0.0f);
                prim.AddVertex2D(0.205f, 0.585f, 1.0f, 1.0f);
                prim.EndDraw();
            }
#endif
            start->Release();
        }

#ifdef DEBUG_SHOW_CAP_RENDER_BUFFER
        if (prim.BeginDraw(end, GFXPT_SCREEN_QUADLIST)) {
            prim.AddVertex2D(0.005f, 0.79f, 0.0f, 1.0f);
            prim.AddVertex2D(0.005f, 0.99f, 0.0f, 0.0f);
            prim.AddVertex2D(0.205f, 0.99f, 1.0f, 0.0f);
            prim.AddVertex2D(0.205f, 0.79f, 1.0f, 1.0f);
            prim.EndDraw();
        }
#endif
        end->Release();
        end = NULL;
    }

    // bounding box and axes
    renderer.SetZTestDisable();
    renderer.SetBlendDisable();
    if (prim.BeginDraw(NULL, GFXPT_LINELIST)) {
        prim.SetColor(64, 64, 64, 255);
        prim.AddVertex( infX_, -supY_,  supZ_);
        prim.AddVertex( infX_, -supY_,  infZ_);
        prim.AddVertex( infX_, -supY_,  infZ_);
        prim.AddVertex( supX_, -supY_,  infZ_);
        prim.AddVertex( supX_, -supY_,  infZ_);
        prim.AddVertex( supX_, -supY_,  supZ_);
        prim.AddVertex( supX_, -supY_,  supZ_);
        prim.AddVertex( infX_, -supY_,  supZ_);
        prim.AddVertex( infX_, -infY_,  supZ_);
        prim.AddVertex( infX_, -infY_,  infZ_);
        prim.AddVertex( infX_, -infY_,  infZ_);
        prim.AddVertex( supX_, -infY_,  infZ_);
        prim.AddVertex( supX_, -infY_,  infZ_);
        prim.AddVertex( supX_, -infY_,  supZ_);
        prim.AddVertex( supX_, -infY_,  supZ_);
        prim.AddVertex( infX_, -infY_,  supZ_);

        prim.AddVertex( infX_, -supY_,  supZ_);
        prim.AddVertex( infX_, -infY_,  supZ_);
        prim.AddVertex( infX_, -supY_,  infZ_);
        prim.AddVertex( infX_, -infY_,  infZ_);
        prim.AddVertex( supX_, -supY_,  infZ_);
        prim.AddVertex( supX_, -infY_,  infZ_);
        prim.AddVertex( supX_, -supY_,  supZ_);
        prim.AddVertex( supX_, -infY_,  supZ_);

        // axes
        prim.SetColor(Color::Red);
        prim.AddVertex(0.0f, 0.0f, 0.0f);
        prim.AddVertex(supX_, 0.0f, 0.0f);

        prim.SetColor(Color::Green);
        prim.AddVertex( 0.0f, 0.0f, 0.0f);
        prim.AddVertex( 0.0f,-infY_, 0.0f);

        prim.SetColor(Color::Blue);
        prim.AddVertex( 0.0f, 0.0f, 0.0f);
        prim.AddVertex( 0.0f, 0.0f, supZ_);

        prim.EndDraw();
    }

    renderer.PopState();

    return true;
}

//---------------------------------------------------------------------------------------
bool VoxelViewer::ShowInfo(mlabs::balai::graphics::IAsciiFont* font, bool verbose)
{
    if (font) {
        Color const color = Color::White;
        char text[128];
        switch (status_)
        {
        case STATUS_EMPTY:
            font->DrawText(0.5f, 0.5f, 24, Color::White, "Ctrl+F to Lead File!", FONT_ALIGN_CENTER);
            break;

        case STATUS_LOADING:
            font->DrawText(0.5f, 0.5f, 24, Color::White, "Loading File...", FONT_ALIGN_CENTER);
            break;

        case STATUS_LOADED:
            font->DrawText(0.5f, 0.5f, 24, Color::White, "File loaded", FONT_ALIGN_CENTER);
            break;

        case STATUS_VOLUMEMAP_UPDATE:
            font->DrawText(0.5f, 0.5f, 24, Color::White, "VOLUMEMAP_UPDATE", FONT_ALIGN_CENTER);
            break;

        case STATUS_READY:
            if (verbose) {
                float y = 0.01f;

                y += font->DrawText(0.01f, y, 16, color, shortName_);

                sprintf(text, "%dx%dx%d", sizeX_, sizeZ_, sizeY_);
                y += font->DrawText(0.01f, y, 16, color, text);
/*
                sprintf(text, "[%.4f, %.4f]x[%.4f, %.4f]x[%.4f, %.4f]",
                        infX_, supX_, infZ_, supZ_, infY_, supY_);
                y += font->DrawText(0.01f, y, 16, color, text);
*/
//                sprintf(text, "time:%.4f", timestamp_);
//                y += font->DrawText(0.01f, y, 16, color, text);

                y += 0.005f;

                //
                //sprintf(text, "debug:%d", toggle_debug);
                //y += font->DrawText(0.01f, y, 16, color, text);
                //

                //
                // ...
                //

#ifdef DEBUG_SHOW_CAP_RENDER_BUFFER
                font->DrawText(0.01f, 0.78f, 16, Color::White, "Front Face", FONT_ALIGN_BOTTOM);
                font->DrawText(0.01f, 0.985f, 16, Color::White, "Back Face", FONT_ALIGN_BOTTOM);
#endif
            }
            break;

        case STATUS_LOADING_FAILED:
            sprintf(text, "Failed to load '%s' Ctrl+F to open new", shortName_);
            font->DrawText(0.5f, 0.5f, 24, Color::White, text, FONT_ALIGN_CENTER);
            break;
        }

        if (STATUS_READY!=status_) {
            if (progressing_>0) {
                sprintf(text, "%s %d%%", message_, progressing_);
                font->DrawText(0.01f, 0.99f, 16, color, text, FONT_ALIGN_BOTTOM);
            }
            else {
                font->DrawText(0.01f, 0.99f, 16, color, message_, FONT_ALIGN_BOTTOM);
            }
        }
    }
    return true;
}

} // namespace sfg