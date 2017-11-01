/* 
 * MIT license
 *
 * Copyright (c) 2017 SFG(Star-Formation Group), Physics Dept., CUHK.
 * All rights reserved.
 *
 * file    SFGVoxelViewer.h
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
#ifndef SFG_VOLUME_VIEWER_H
#define SFG_VOLUME_VIEWER_H

#include "SFGPVMParser.h"

#include "BLGLShader.h"
#include "BLOpenGL.h"
#include "BLRenderSurface.h"
#include "BLFont.h"
#include "BLMatrix3.h"

//#include <condition_variable>
//#include <future>
#include <thread>
//#include <mutex>
#include <atomic>

namespace sfg {

// sfg renderable cube
class VoxelViewer : public PVMParser
{
    enum STATUS {
        STATUS_EMPTY, // volume map not loaded
        STATUS_LOADING, // now loading PVM
        STATUS_LOADED, // PVM loaded
        STATUS_READY, // ready to render
        STATUS_VOLUMEMAP_UPDATE,
        STATUS_LOADING_FAILED, // oh no!
    };

    size_t              histogram_[1024];
    char                fullName_[256];
    char                message_[256];
    std::thread         thread_;
    std::atomic<STATUS> status_;

    uint8*              voxels_;


    mlabs::balai::graphics::ShaderEffect* tcFx_;
    mlabs::balai::graphics::shader::Constant const* matTC_;
    mlabs::balai::graphics::shader::Constant const* clipping_;
    
    // effects
    mlabs::balai::graphics::ShaderEffect* rayMarching_;
    mlabs::balai::graphics::shader::Constant const* step_;
    mlabs::balai::graphics::shader::Sampler const* ray_start_;
    mlabs::balai::graphics::shader::Sampler const* ray_end_;
    mlabs::balai::graphics::shader::Sampler const* voxel_;

    // front and back surface
    mlabs::balai::graphics::RenderSurface* frontCap_;
    mlabs::balai::graphics::RenderSurface* backCap_;

    // volume map
    mlabs::balai::graphics::Texture3D* volumeMap_;

    // file short name
    char const* shortName_;

    // vertex array buffer/vertex buffer object for volume rendering(quad slices)
    GLuint      vao_, vbo_;
    int         slices_;

    // loading
    int         progressing_;

    //
    int         sizeX_, sizeY_, sizeZ_;
    float       infX_, infY_, infZ_;
    float       supX_, supY_, supZ_;
    float       scaleX_, scaleY_, scaleZ_; // scale

    float       voxInf_, voxSup_;

    // PVM voxel
    void ReadVoxels_(uint8 const* voxels,
                     int width, int height, int depth, int components,
                     float sizeW, float sizeH, float sizeD,
                     char const* description,
                     char const* courtesy,
                     char const* parameters,
                     char const* comment);
    // load pvm
    void LoadPVM_();

public:
    VoxelViewer():thread_(),status_(STATUS_EMPTY),
        tcFx_(NULL),matTC_(NULL),clipping_(NULL),

        rayMarching_(NULL),step_(NULL),ray_start_(NULL),ray_end_(NULL),voxel_(NULL),

        frontCap_(NULL),backCap_(NULL),
        volumeMap_(NULL),
        shortName_(NULL),
        vao_(0),vbo_(0),
        slices_(0),
        progressing_(0),
        sizeX_(0),sizeY_(0),sizeZ_(0),
        infX_(0.0f),infY_(0.0f),infZ_(0.0f),
        supX_(0.0f),supY_(0.0f),supZ_(0.0f),
        scaleX_(1.0f),scaleY_(1.0f),scaleZ_(1.0f) {

        memset(fullName_, 0, sizeof(fullName_));
        shortName_ = fullName_;
    }
    virtual ~VoxelViewer() {
        status_ = STATUS_EMPTY;
        if (thread_.joinable()) {
            thread_.join();
        }
        if (voxels_) {
            free(voxels_);
            voxels_ = NULL;
        }
    }

    //char const* FullName() const { return fullName_; }
    //char const* ShortName() const { return shortName_; }
    bool IsReady() const { return STATUS_READY==status_; }

    bool LoadPVM(char const* filename, bool async) {
        if (filename) {
            // reset
            if (STATUS_READY==status_) {
                status_ = STATUS_EMPTY;
            }
            else if (STATUS_LOADING==status_ || STATUS_LOADED==status_ || STATUS_LOADING_FAILED==status_) {
                status_ = STATUS_EMPTY;
                if (thread_.joinable()) {
                    thread_.join();
                }
                status_ = STATUS_EMPTY;
            }

            if (STATUS_EMPTY==status_) {
                status_ = STATUS_LOADING;
                strncpy(fullName_, filename, 255);
                shortName_ = strrchr(fullName_, '/');
                if (NULL == shortName_) {
                    shortName_ = strrchr(fullName_, '\\');
                    shortName_ = (NULL == shortName_) ? fullName_ : (shortName_ + 1);
                }
                else {
                    char const* s = strrchr(fullName_, '\\');
                    shortName_ = (shortName_>s) ? (shortName_ + 1) : (s + 1);
                }

                if (async) {
                    thread_ = std::move(std::thread([this] { LoadPVM_(); }));
                    return true; // too early to say...
                }
                else {
                    LoadPVM_();
                }
            }
        }
        return (STATUS_LOADED==status_);
    }

    bool Initialize();

    bool FrameUpdate(float frameTime);

    bool Render(mlabs::balai::math::Matrix3 const& matModelView);

    void Finalize();

    bool ShowInfo(mlabs::balai::graphics::IAsciiFont* font, bool verbose);
};


} // namespace sfg

#endif