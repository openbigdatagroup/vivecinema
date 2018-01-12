/*
 * Copyright (C) 2017 HTC Corporation
 *
 * Vive Cinema is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Vive Cinema is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Notice Regarding Standards. HTC does not provide a license or sublicense to
 * any Intellectual Property Rights relating to any standards, including but not
 * limited to any audio and/or video codec technologies such as MPEG-2, MPEG-4;
 * AVC/H.264; HEVC/H.265; AAC decode/FFMPEG; AAC encode/FFMPEG; VC-1; and MP3
 * (collectively, the "Media Technologies"). For clarity, you will pay any
 * royalties due for such third party technologies, which may include the Media
 * Technologies that are owed as a result of HTC providing the Software to you.
 *
 * @file    main.cpp
 * @author  andre chen, andre.HL.chen@gmail.com
 * @history 2016/03/15 created
 *
 */
#include "BLApplication.h"
#include "BLCStdMemory.h"
#include "BLPrimitives.h"
#include "BLFont.h"

#include "BLFileStream.h"

#ifndef HTC_VIVEPORT_RELEASE
#include "BLJPEG.h" // app icon from jpeg
#endif

#include "VRVideoPlayer.h"

BL_CSTD_MEMMGR_IMPLMENTATION(64<<20, 1<<20);

using namespace mlabs::balai::graphics;
using namespace mlabs::balai::math;

namespace mlabs { namespace balai { namespace framework {

/*
 * versions -
 *   2016.03.15 ver 0.0.001 - Day1
 *   2016.04.16 ver 0.0.003
 *   2016.04.22 ver 0.1.000
 *   2016.04.25 ver 0.1.042 - Day42
 *   2016.05.02 ver 0.1.049
 *   2016.05.03 ver 0.1.050
 *   2016.05.04 ver 0.1.051
 *   2016.05.06 ver 0.1.053
 *   2016.05.09 ver 0.1.056
 *   2016.05.12 ver 0.1.059
 *   2016.05.17 ver 0.1.064 - 1st viveport submit (2016.05.19 on Viveport)
 *   2016.05.27 ver 0.2.074 - (2nd submit) optimization - NV21 texture, new decoder
 *   2016.06.01 ver 0.2.079 - (3rd submit, no more DRM) tweak 360/SBS
 *   2016.06.17 ver 0.3.095 - (4th submit) 360/180/Plane x MONO/SBS/TB = 9 modes
 *   2016.07.05 ver 0.4.113 - (5th submit) mpeg decoder optimization
 *   2016.09.05 ver 0.5.175 - language(audio) and embedded subtitle.
 *   2016.09.07 ver 0.5.177 - update FFmpeg libraries ffmpeg-20160906-496d97f
 *   2016.09.26 ver 0.5.196 - embeded ASS/SSA subtitle working
 *   2016.10.07 ver 0.5.207 - SRT/ASS/SSA all working, but still have ASS/SSA alingment and encoding problems
 *   2016.10.11 ver 0.5.211 - all subtitle text convert to utf-8
 *   2016.10.14 ver 0.5.214 - (6th submit) subtitle + multi-language support
 *   2016.11.11 ver 0.6.242 - ASS animation(\t), 360 subtitle, fix audio sync problem (when audio changes massively), FFmpeg 3.1.5
 *   2016.11.24 ver 0.6.255 - optimize subtitle rendering.
 *   2016.12.16 ver 0.7.277 - ambisonic audio
 *   2016.12.20 ver 0.7.281 - (7th submit) make 360 sutitle follow.
 *   2017.01.05 ver 0.8.297 - (KKBOX) Live Streaming for the 12th KKBOX Music Awards 2017.01.21
 *   2017.01.10 ver 0.8.302 - (KKBOX) internet traffic handling
 *   2017.01.20 ver 0.8.312 - (KKBOX) final release
 *   2017.01.25 ver 0.8.317 - embarrassing live-streaming bugs fixed.
 *   2017.03.14 ver 0.8.365 - new decoding process. audio+subtitle thread, extended audio streams
 *   2017.03.27 ver 0.8.378 - fix wmv audio pts problems
 *   2017.04.06 ver 0.8.388 - (8th submit) xml setting file for livestreams, pathes and videos
 *   2017.06.29 ver 0.9.472 - use SADIE KU100 HRIR for ambisonics binaural decoder
 *   2017.07.05 ver 0.9.478 - (9th submit)
 *   2017.07.06 ver 0.9.479 - single view
 *   2017.07.07 ver 0.9.480 - migrate to FFmpeg 3.3.2
 *   2017.07.27 ver 0.9.500 - 5.1/7.1 audio HRTF
 *   2017.07.28 ver 0.9.501 - (10th submit)
 *   2017.08.15 ver 0.9.501+ - (11th submit) rollback openvr sdk!
 *   2017.08.28 ver 0.9.531 - modify Rec.709 color conversion (no gamma correction) for Deserted @ Venice
 *   2017.09.12 ver 0.9.546 - Hardware accelerated video decoder, AMD AMF and NVIDIA CUVID integration
 *   2017.09.25 ver 0.9.559 - a build for Kaohsiung Film Festival
 *   2017.09.27 ver 0.9.561 - loop(repeat) version (for Golden Bell Awards event)
 *   2017.09.28 ver 0.9.562 - final RC. ready to open source. waiting for final video from VRC Jack.
 *   2017.09.29 ver 0.9.563 - GPL3 license, open source https://github.com/openbigdatagroup/vivecinema
 *   2017.11.30 ver 0.9.624 - (12th <the last?> submit) fix subtitle sleep, remove async.
 *   2017.12.08 ver 0.9.632 - Fix NVDEC hevc main10/12 profile GP10x GPU
 *   2017.12.20 ver 0.9.644 - CUDA/OpenGL interop. frame rate isn't a problem anymore! testing...
 *   2017.12.25 ver 0.9.649 - fix texture failed to update with CUDA/OpenGL interoperability
 *   2018.01.11 ver 0.9.666 - UI/minor fixes
 */
char const* const buildNo = "Vive Cinema Build 0.9.666";

#define DRAW_SINGLE_VIEW
#ifdef DRAW_SINGLE_VIEW
// reference width x height = 1512x1680
#define DRAW_SINGLE_VIEW_WIDTH 1440
//#define DRAW_SINGLE_VIEW_HEIGHT 900
#endif

#ifdef OPTIMIZING_SHOW_FRAME_TIMIING
int const FRAME_TIMING_LOG_FRAMES = 180;
static int cur_log_frame = 0;
static double missed_frames[FRAME_TIMING_LOG_FRAMES];
static double timevsync_vrpose[FRAME_TIMING_LOG_FRAMES];
static double vrpose_update[FRAME_TIMING_LOG_FRAMES];
static double pbo_update[FRAME_TIMING_LOG_FRAMES];
static double drawcall_submit[FRAME_TIMING_LOG_FRAMES];
static double vrpresent[FRAME_TIMING_LOG_FRAMES];
static double baseline[FRAME_TIMING_LOG_FRAMES];
static uint64 last_frame_id = 0;

void DrawHistogram(Primitives& prim, float x0, float /*y0*/, float x1, float y1,
                   Color const& clr, double const* histogram, int curr,
                   float y_scale=1.0f, float y_bias=0.0f) {
    prim.BeginDraw(NULL, GFXPT_SCREEN_LINELIST);
    prim.SetColor(clr);
    int const offset = curr + 1;
    float const dx = (x1 - x0)/(FRAME_TIMING_LOG_FRAMES-1);
    y1 += y_bias;
    for (int i=0; i<FRAME_TIMING_LOG_FRAMES; ++i) {
        float const t = (float) histogram[(i + offset)%FRAME_TIMING_LOG_FRAMES];
        if (t!=0.0f) {
            float const x = x0 + dx * i;
            float const y = y1 - y_scale*t;
            prim.AddVertex2D(x, y1);
            prim.AddVertex2D(x, y);
        }
    }
    prim.EndDraw();
}
void DrawHistogram(Primitives& prim, float x0, float x1, float scale, float y_base,
                   Color const& clr, double* base, double const* histogram, int curr) {
    prim.BeginDraw(NULL, GFXPT_SCREEN_LINELIST);
    prim.SetColor(clr);
    int const offset = curr + 1;
    float const dx = (x1 - x0)/(FRAME_TIMING_LOG_FRAMES-1);
    for (int i=0; i<FRAME_TIMING_LOG_FRAMES; ++i) {
        int const id = (i + offset)%FRAME_TIMING_LOG_FRAMES;
        float const t = (float) histogram[id];
        if (t!=0.0f) {
            float const x = x0 + dx * i;
            float const y = y_base - float(base[id]*scale);
            prim.AddVertex2D(x, y);
            prim.AddVertex2D(x, y-scale*t);
            base[id] += histogram[id];
        }
    }
    prim.EndDraw();
}
void DrawCurve(Primitives& prim, float x0, float /*y0*/, float x1, float y1,
               Color const& clr, double const* curve, int curr,
               float y_scale=1.0f, float y_bias=0.0f) {
    prim.BeginDraw(NULL, GFXPT_SCREEN_LINESTRIP);
    prim.SetColor(clr);
    int const offset = curr + 1;
    float const dx = (x1 - x0)/(FRAME_TIMING_LOG_FRAMES-1);
    for (int i=0; i<FRAME_TIMING_LOG_FRAMES; ++i) {
        float const t = (float) curve[(i + offset)%FRAME_TIMING_LOG_FRAMES] - y_bias;
        float const x = x0 + dx * i;
        float const y = y1 - y_scale*t;
        prim.AddVertex2D(x, y);
    }
    prim.EndDraw();
}
#endif

class MyApp : public BaseApp
{
    WCHAR          videoPath_[MAX_PATH];
    htc::VRVideoPlayer player_;
    VR::Manager&   vrMgr_;
    IAsciiFont*    font_;
    RenderSurface* surfaceL_;
    RenderSurface* surfaceR_;
    uint8          multisampleSamples_;
    uint8          msaa_enable_;
    uint8          display_info_;
    uint8          perf_HUD_pause_;

    uint16* LoadIconData_(int& width, int& height) {
        fileio::ifstream stream;
        if (stream.Open("./assets/asset.bin")) {
            fileio::FileHeader header;
            fileio::FileChunk chunk;
            stream >> header;
            if (0==memcmp(header.Description , "Vive Cinema 360", 15)) {
                uint32 const icon = (('I'<<24)|('C'<<16)|('O'<<8)|('N'));
                do {
                    stream >> chunk;
                    if (chunk.ID==icon) {
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
#ifndef HTC_VIVEPORT_RELEASE
        else {
            int channels = 0;
            void* pixels = mlabs::balai::image::read_JPEG("./assets/vive_64.jpg", width, height, channels);
            if (pixels) {
                uint16* dst = (uint16*) pixels;
                uint8 const* src = (uint8 const*) pixels;
                int const total_pixels = width*height;
                for (int i=0; i<total_pixels; ++i,src+=channels) {
                    *dst++ = ((src[0]&0xf8)<<8) | ((src[1]&0xfc)<<3) | ((src[2]&0xf8)>>3);
                }
            }
            return (uint16*) pixels;
        }
#endif
        return NULL;
    }
    void FreeIconData_(void* pixels, int, int) {
        free(pixels);
    }

public:
    MyApp():vrMgr_(VR::Manager::GetInstance()),
        player_(),
        font_(NULL),
        surfaceL_(NULL),surfaceR_(NULL),
        multisampleSamples_(8),msaa_enable_(0),
        display_info_(1),
        perf_HUD_pause_(0) {

        //
        // 2016.05.18
        // the current directory is viveport launcher's directory. change it!!!
#ifdef HTC_VIVEPORT_RELEASE
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
        window_title_ = "Vive Cinema";

        int width(0), height(0);
        if (vrMgr_.GetRenderSurfaceSize(width, height)) {
#ifdef DRAW_SINGLE_VIEW
 #ifdef DRAW_SINGLE_VIEW_WIDTH
            width_  = (uint16) DRAW_SINGLE_VIEW_WIDTH;
  #ifdef DRAW_SINGLE_VIEW_HEIGHT
            height_ = (uint16) DRAW_SINGLE_VIEW_HEIGHT;
  #else
            height_ = (uint16) (width_*9/16);
  #endif
 #else
            width_  = (uint16) width;
            height_ = (uint16) (width_*9/16);
 #endif
#else
            width_  = (uint16) width;
            height_ = (uint16) height/2;
#endif
            // cannot fit!?
            int const max_width = GetSystemMetrics(SM_CXSCREEN);
            int const max_height = GetSystemMetrics(SM_CYSCREEN);
            if (0<max_width && 0<max_height &&
                (10*width_>9*max_width || 10*height_>9*max_height)) {
                // keep aspect ratio
                int const max_height2 = max_width*height_/width_;
                if (max_height2>max_height) {
                    height = max_height*90/100;
                    width_ = (uint16) (height*width_/height_);
                    height_ = (uint16) height;
                }
                else {
                    width = (uint16) max_width*90/100;
                    height_ = (uint16) width*height_/width_;
                    width_ = (uint16) width;
                }
            }

            vsync_on_ = 0; // do NOT waste anytime on main window vsync.
        }
        else {
            ::MessageBoxA(NULL, "Sorry...\nOpenVR system isn't ready, App must quit.", window_title_, MB_OK);
        }

        // invalid video path
        videoPath_[0] = L'\0';
    }
    void ParseCommandLine(int argc, char* argv[]) {
        if (argc>1) {
            for (int i=0; i<argc; ++i) {
                char const* arg = argv[i];
                if (NULL!=arg && '-'==arg[0] &&
                    'p'==arg[1] && 'a'==arg[2] && 't'==arg[3] && 'h'==arg[4] && (i+1)<argc) {
                    for (int j=i+1; j<argc; ++j) {
                        char const* arg2 = argv[j];
                        if (NULL!=arg2 && 
                            'v'==arg2[0] &&
                            'i'==arg2[1] &&
                            'd'==arg2[2] &&
                            'e'==arg2[3] &&
                            'o'==arg2[4] &&
                            '='==arg2[5]) {
                            // CP_ACP : The system default Windows ANSI code page.
                            int len = MultiByteToWideChar(CP_ACP, 0, arg2+6, -1, videoPath_, sizeof(videoPath_)/sizeof(videoPath_[0]));
                            videoPath_[len] = 0;
                            //mbstowcs(videoPath_, arg2+6, 256);
                            break;
                        }
                    }
                    break;
                }
            }
        }
    }
    bool BuildStereoRenderSurfaces(uint8 samples) {
        int width(0), height(0);
        if (!vrMgr_.GetRenderSurfaceSize(width, height)) {
#ifdef BL_DEBUG_BUILD
            width = 1512; // i guess
            height = 1680;
#else
            return false;
#endif
        }

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

    bool Initialize() {
        msaa_enable_ = 1;
        multisampleSamples_ = (uint8) Renderer::GetInstance().GetMaxMultisampleSamples();
        if (multisampleSamples_>8) multisampleSamples_ = 8;
        if (!BuildStereoRenderSurfaces(multisampleSamples_)) {
            return false;
        }

        // a font for debugging purpose
        font_ = IAsciiFont::CreateFontFromFile("./assets/Gotham.font");

        // init VR mgr and do first pose update
        vrMgr_.Initialize();
        vrMgr_.SetClippingPlanes(0.1f, 100.0f);
        vrMgr_.UpdatePoses();

        // init video player
        if (player_.Initialize()) {
            WCHAR const* auxPath = L"D:/Vive Cinema";
            WCHAR targetPath[MAX_PATH];
            if (videoPath_[0]==L'\0') {
                htc::Win32KnownFolderPath dir;
                WCHAR const* path = dir.GetPath(htc::KNOWN_PATH_VIDEOS);
                if (NULL!=path)  {
                    swprintf(targetPath, MAX_PATH, L"%s\\Vive Cinema", path);
                }
                else { // if cannot locate Library/video...
                    wcscpy_s(targetPath, MAX_PATH, auxPath);
                }
            }
            else {
                wcscpy_s(targetPath, videoPath_);
            }

            //
            // GetFileAttributes() may fail for other reasons than lack of existence 
            // (for example permissions issues). I would add a check on the error code for robustness
            DWORD const mainPathAttrib = GetFileAttributes(targetPath);
            int path_exist = (mainPathAttrib!=INVALID_FILE_ATTRIBUTES) &&
                              (GetLastError()!=ERROR_FILE_NOT_FOUND) &&
                              (0!=(mainPathAttrib&FILE_ATTRIBUTE_DIRECTORY));
            if (!path_exist) {
                if (CreateDirectory(targetPath, NULL)) {
                    path_exist = 2; // new created
                }

                // only create for the 1st time... prevent repeatly creating
                // this directory after users delete it.
                DWORD const auxPathAttrib = GetFileAttributes(auxPath);
                int auxPath_exist = (auxPathAttrib!=INVALID_FILE_ATTRIBUTES) &&
                                    (GetLastError()!=ERROR_FILE_NOT_FOUND) &&
                                    (0!=(auxPathAttrib&FILE_ATTRIBUTE_DIRECTORY));
                if (!auxPath_exist && CreateDirectory(auxPath, NULL)) {
                    auxPath_exist = 2;
                }

                if (!path_exist && auxPath_exist) {
                    wcscpy_s(targetPath, auxPath);
                    path_exist = 3;
                }
            }

            if (path_exist) {
                // vivecinema.xml -- don't not overwrite user's own copy!
                WCHAR wfilename[256];
                swprintf(wfilename, 256, L"%s/%s", targetPath, L"vivecinema.xml");
                DWORD const ret = GetFileAttributesW(wfilename);
                if (INVALID_FILE_ATTRIBUTES==ret || ERROR_FILE_NOT_FOUND==GetLastError()) {
                    CopyFileW(L"./assets/vivecinema.xml", wfilename, TRUE); // fail if exists
                }

#ifdef HTC_VIVEPORT_RELEASE
                if (2==path_exist||3==path_exist) {
                    // if we can create this directory, copy embeded videos now.
                    WIN32_FIND_DATAW fd;
                    HANDLE hFind = FindFirstFileW(L"./assets/*.*", &fd);
                    if (INVALID_HANDLE_VALUE!=hFind) {
                        WCHAR wNewFilename[256];
                        do {
                            if (0==(FILE_ATTRIBUTE_DIRECTORY&fd.dwFileAttributes)) {
                                size_t short_len = wcslen(fd.cFileName);
                                if (short_len>4) {
                                    size_t full_len = 0;
                                    wchar_t const* ext = fd.cFileName + short_len - 4;
                                    if (*ext==L'.') {
                                        ++ext;
                                        if (0==memcmp(ext, L"mp4", 3*sizeof(wchar_t)) || 0==memcmp(ext, L"MP4", 3*sizeof(wchar_t)) ||
                                            0==memcmp(ext, L"mov", 3*sizeof(wchar_t)) || 0==memcmp(ext, L"MOV", 3*sizeof(wchar_t)) ||
                                            0==memcmp(ext, L"mkv", 3*sizeof(wchar_t)) || 0==memcmp(ext, L"MKV", 3*sizeof(wchar_t)) ||
                                            0==memcmp(ext, L"srt", 3*sizeof(wchar_t)) || 0==memcmp(ext, L"SRT", 3*sizeof(wchar_t)) ||
                                            0==memcmp(ext, L"ass", 3*sizeof(wchar_t)) || 0==memcmp(ext, L"ASS", 3*sizeof(wchar_t))) {
                                            full_len = swprintf(wfilename, 256, L"./assets/%s", fd.cFileName);
                                        }
                                    }
                                    else if (short_len>5 && ext[-1]==L'.') {
                                        if (0==memcmp(ext, L"divx", 4*sizeof(wchar_t))) {
                                            full_len = swprintf(wfilename, 256, L"./assets/%s", fd.cFileName);
                                        }
                                    }

                                    if (full_len>=14) {
                                        swprintf(wNewFilename, 256, L"%s/%s", targetPath, fd.cFileName);
                                        CopyFile(wfilename, wNewFilename, TRUE); // fail if exists
                                    }
                                }
                            }
                        } while (0!=FindNextFileW(hFind, &fd));

                        FindClose(hFind);
                    }
                }
#endif
            }

            player_.SetMediaPath(targetPath);

#ifdef BL_DEBUG_BUILD
            blDumpMemory();
            blDumpFreeMemory();
#endif
            return true;
        }
        return false;
    }
    bool FrameMove(float /*updateTime*/) {
        if (msaa_enable_>1) {
            msaa_enable_ &= 1;
            BuildStereoRenderSurfaces(msaa_enable_ ? multisampleSamples_:0);
        }
        return true;
    }
    bool DrawScene(VR::HMD_EYE eye) {
        bool ok = false;
        Renderer& renderer = Renderer::GetInstance();
        renderer.SetSurface((VR::HMD_EYE_LEFT==eye) ? surfaceL_:surfaceR_);
        renderer.Clear(Color::Gray);
        if (renderer.BeginScene()) {
            renderer.SetViewMatrix(vrMgr_.GetViewMatrix(eye));
            renderer.SetProjectionMatrix(vrMgr_.GetProjectionMatrix(eye));
            ok = player_.Render(eye);
            renderer.EndScene();
        }
        return ok;
    }
#ifdef OPTIMIZING_SHOW_FRAME_TIMIING
    bool Render_FrameTiming() {
        memset(baseline, 0, sizeof(baseline));
        double t0 = 0.0;

        t0 = system::GetTime();
        vrMgr_.Present(VR::HMD_EYE_LEFT, surfaceL_);
        vrMgr_.Present(VR::HMD_EYE_RIGHT, surfaceR_);
        vrpresent[cur_log_frame] = (system::GetTime() - t0);

        t0 = system::GetTime();
        player_.FrameMove();
        pbo_update[cur_log_frame] = (system::GetTime() - t0);

        t0 = system::GetTime();
        vrMgr_.UpdatePoses();
        vrpose_update[cur_log_frame] = (system::GetTime() - t0);

        uint64 frameIdUpdatePose = 0;
        timevsync_vrpose[cur_log_frame] = vrMgr_.GetTimeSinceLastVsync(frameIdUpdatePose) - 0.01111f;
        assert(last_frame_id<=frameIdUpdatePose);
        int const frames = int(frameIdUpdatePose-last_frame_id-1);/*
        for (int i=0; i<frames; ++i) {
            int id = (i+cur_log_frame)%FRAME_TIMING_LOG_FRAMES;
            vrpresent[id] = vrpresent[cur_log_frame]; vrpresent[cur_log_frame] = 0.0;
            missed_frames[id] = missed_frames[cur_log_frame]; missed_frames[cur_log_frame] = 0.0;
            timevsync_vrpose[id] = timevsync_vrpose[cur_log_frame]; timevsync_vrpose[cur_log_frame] = 0.0;
            vrpose_update[id] = vrpose_update[cur_log_frame]; vrpose_update[cur_log_frame] = 0.0;
            pbo_update[id] = pbo_update[cur_log_frame]; pbo_update[cur_log_frame] = 0.0;
            drawcall_submit[id] = drawcall_submit[cur_log_frame]; drawcall_submit[cur_log_frame] = 0.0;

            if (!meter_pause)
                cur_log_frame = (cur_log_frame+1)%FRAME_TIMING_LOG_FRAMES;
        }*/

        missed_frames[cur_log_frame] = 0.001f*frames;
        last_frame_id = frameIdUpdatePose;

        // renderer
        Renderer& renderer = Renderer::GetInstance();

        // design flaw...
        // must set back default renderbuffer before both present(surfaceR_)
        renderer.SetSurface(NULL);

        // 2) draw next frames to be presented in the next loop
        t0 = system::GetTime();
        bool const ret = DrawScene(VR::HMD_EYE_LEFT) && DrawScene(VR::HMD_EYE_RIGHT);
        drawcall_submit[cur_log_frame] = (system::GetTime() - t0);
/*
        // bad performance for Radeon R9 390, if present here...
        t0 = system::GetTime();
        vrMgr_.Present(VR::HMD_EYE_LEFT, surfaceL_);
        vrMgr_.Present(VR::HMD_EYE_RIGHT, surfaceR_);
        vrpresent[cur_log_frame] = (system::GetTime() - t0);
*/
        renderer.PushState();
        renderer.SetSurface(NULL);
        renderer.Clear(Color::Cyan);
        renderer.SetDepthMask(true);
        renderer.CommitChanges();
        if (renderer.BeginScene()) {
            Primitives& prim = Primitives::GetInstance();
            ITexture* tex = surfaceL_->GetRenderTarget(0);
#ifdef DRAW_SINGLE_VIEW
            if (tex) {
                if (prim.BeginDraw(tex, GFXPT_SCREEN_QUADLIST)) {
                    float const dst_aspect_ratio = renderer.GetScreenAspectRatio();
                    float const src_aspect_ratio = tex->Width()/(float)tex->Height();
                    float x0(0.0f), x1(1.0f), y0(0.0f), y1(1.0f);
                    if (src_aspect_ratio<dst_aspect_ratio) {
                        y1 = src_aspect_ratio/dst_aspect_ratio;
                        y0 = 0.5f - 0.5f*y1;
                        y1 += y0;
                    }
                    else if (src_aspect_ratio>dst_aspect_ratio) {
                        x1 = dst_aspect_ratio/src_aspect_ratio;
                        x0 = 0.5f - 0.5f*x1;
                        x1 += x0;
                    }
                    prim.AddVertex2D(0.0f, 0.0f, x0, y1);
                    prim.AddVertex2D(0.0f, 1.0f, x0, y0);
                    prim.AddVertex2D(1.0f, 1.0f, x1, y0);
                    prim.AddVertex2D(1.0f, 0.0f, x1, y1);
                    prim.EndDraw();
                }
                tex->Release();
                tex = NULL;
            }
#else
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
#endif
            //
            // draw glyph...
            //

            float const x0 = 0.13f;
            float const x1 = 0.96f;
            float const y0 = 0.20f;
            float const y1 = 0.75f;
            float const jump1ms = (y1-y0)/11.11f;
            Color clr = Color::White;
            renderer.SetBlendMode(GFXBLEND_SRCALPHA, GFXBLEND_INVSRCALPHA);
            prim.BeginDraw(NULL, GFXPT_SCREEN_QUADLIST);
                clr.a = 96;
                prim.SetColor(clr);
                prim.AddVertex2D(x0, y0);
                prim.AddVertex2D(x0, y1);
                prim.AddVertex2D(x1, y1);
                prim.AddVertex2D(x1, y0);
            prim.EndDraw();

            prim.BeginDraw(NULL, GFXPT_SCREEN_LINELIST);
                clr = Color::White; 
                prim.SetColor(clr);
                prim.AddVertex2D(x0, y1);
                prim.AddVertex2D(x1, y1);
                float y = y1 - jump1ms*11.11f;
                prim.AddVertex2D(x0, y);
                prim.AddVertex2D(x1, y);
                clr.a = 128;
                prim.SetColor(clr);
                for (int i=-4; i<12; ++i) {
                    if (i!=0) {
                        y = y1 - jump1ms*i;
                        prim.AddVertex2D(x0, y);
                        prim.AddVertex2D(x1, y);
                    }
                    else {
                        clr = Color::Black; clr.a = 128;
                        prim.SetColor(clr);
                    }
                }
            prim.EndDraw();

            renderer.SetBlendDisable();

            // update pose, "Running Start"
            DrawCurve(prim, x0, y0, x1, y1,
                      Color::Cyan, timevsync_vrpose, cur_log_frame,
                      1000.0f*jump1ms, 0.0f);

            // drawcalls
            DrawHistogram(prim, x0, x1, 1000.0f*jump1ms, y1, Color::Green, baseline, drawcall_submit, cur_log_frame);

            // present
            DrawHistogram(prim, x0, x1, 1000.0f*jump1ms, y1, Color::Blue, baseline, vrpresent, cur_log_frame);

            // pbo update - main frame time
            DrawHistogram(prim, x0, x1, 1000.0f*jump1ms, y1, Color::Yellow, baseline, pbo_update, cur_log_frame);

            // wait get pose
            DrawHistogram(prim, x0, x1, 1000.0f*jump1ms, y1, Color::Purple, baseline, vrpose_update, cur_log_frame);

            // missed frame
            DrawHistogram(prim, x0, y0, x1, y1,
                          Color::Red, missed_frames, cur_log_frame,
                          -1000.0f*jump1ms, 0.0f);

            if (font_) {
                int const font_size = 16;
                Color const font_color = Color::White;
                char msg[160];
                y = 0.01f;
                font_->DrawText(0.99f, y, font_size, Color::Yellow, "[Toggle Display:D]", FONT_ALIGN_RIGHT);
                std::sprintf(msg, "FPS : %d", int(GetFPS()+0.5f));
                float const dy = font_->DrawText(0.01f, y, 15, font_color, msg);
                y += dy;
                if (player_.IsLoaded()) {
                    int w, h, ms, s, m;
                    player_.GetVideoWidthHeight(w, h);
                    std::sprintf(msg, "Resolution : %dx%d  %.2f fps", w, h, player_.VideoFrameRate());
                    y += font_->DrawText(0.01f, y, font_size, font_color, msg);

                    int const duration = player_.Duration();
                    int videotime = player_.VideoTime(); if (videotime<0) videotime = 0;
                    int audiotime = player_.AudioTime(); if (audiotime<0) audiotime = 0;
                    if (duration>0) {
                        ms = duration%1000;
                        s  = duration/1000;
                        m  = s/60; s %= 60;
                        h  = m/60; m %= 60;
                        std::sprintf(msg, "Length : %02d:%02d:%02d:%02d", h, m, s, ms/10);
                        font_->DrawText(0.01f, y, font_size, font_color, msg);
                    }
                    else {
                        char const* p = "...";
                        std::sprintf(msg, "Length : live streaming%s", p + (3-((videotime/250)%4)));
                        font_->DrawText(0.01f, y, font_size, font_color, msg);
                    }

                    float const buffer_pos = 0.158f*1600.0f/width_;
                    float const packet_pos = 0.228f*1600.0f/width_;

                    AUDIO_TECHNIQUE tech = AUDIO_TECHNIQUE_DEFAULT;
                    char const* audioinfo = player_.GetAudioInfo(tech);

                    if (audioinfo) {
                        int out_of_sync = (videotime-audiotime)/100;
                        if (out_of_sync>5) out_of_sync = 5;
                        else if (out_of_sync<-5) out_of_sync = -5;
                        memset(msg, '-', 11);
                        msg[out_of_sync + 5] = '+';
                        msg[11] = '\0';
                        y += font_->DrawText(buffer_pos, y, font_size, font_color, msg);
                    }
                    else {
                        y += dy;
                    }

                    int packets(0), sn(0);
                    int vb = player_.VideoBuffering(packets, sn);
                    if (vb>0) {
                        if (vb>10) vb = 10;
                        memset(msg, '+', vb);
                        msg[vb] = '\0';
                        font_->DrawText(buffer_pos, y, font_size, font_color, msg);
                        if (packets>0) {
                            if (packets>120) packets = 120;
                            int c = 0;
                            for (int i=0; i<packets; ++i) {
                                msg[c++] = ((sn++)%100) ? '-':'+';
                                if (0==(i+1)%10)
                                    msg[c++] = ' ';
                            }
                            msg[c] = '\0';
                            font_->DrawText(packet_pos, y, font_size, font_color, msg);
                        }
                    }

                    ms = videotime%1000;
                    s  = videotime/1000;
                    m  = s/60; s %= 60;
                    h  = m/60; m %= 60;
                    std::sprintf(msg, "Video : %02d:%02d:%02d:%02d", h, m, s, ms/10);
                    y += font_->DrawText(0.01f, y, font_size, font_color, msg);

                    int ab = player_.AudioBuffering(packets, sn); // millisecond
                    if (ab>0) {
                        if (ab>800) ab = 800;
                        ab /= 100;
                        memset(msg, '+', ab);
                        msg[ab] = '\0';
                        font_->DrawText(buffer_pos, y, font_size, font_color, msg);
                        if (packets>0) {
                            if (packets>120) packets = 120;
                            int c = 0;
                            for (int i=0; i<packets; ++i) {
                                msg[c++] = ((sn++)%100) ? '-':'+';
                                if (0==(i+1)%10)
                                    msg[c++] = ' ';
                            }
                            msg[c] = '\0';
                            font_->DrawText(packet_pos, y, font_size, font_color, msg);
                        }
                    }

                    if (audioinfo) {
                        ms = audiotime%1000;
                        s  = audiotime/1000;
                        m  = s/60; s %= 60;
                        h  = m/60; m %= 60;
                        std::sprintf(msg, "Audio : %02d:%02d:%02d:%02d", h, m, s, ms/10);
                        y+=font_->DrawText(0.01f, y, font_size, font_color, msg);
                    }
                    else {
                        y+=font_->DrawText(0.01f, y, font_size, font_color, "Audio : N/A");
                    }

                    int extStreamId(0), start(0), subduration(0);
                    bool hardsub = false;
                    int const streamId = player_.SubtitleBuffering(extStreamId, hardsub, start, subduration);
                    if (streamId>=0) {
                        if (subduration>0) {
                            ms = start;
                            s  = ms/1000; ms %= 1000;
                            m  = s/60; s %= 60;
                            h  = m/60; m %= 60;
                            int ms2 = start + subduration;
                            int s2  = ms2/1000; ms2 %= 1000;
                            int m2  = s2/60; s2 %= 60;
                            int h2  = m2/60; m2 %= 60;
                            std::sprintf(msg, "Subtitle : %02d:%02d:%02d:%02d --> %02d:%02d:%02d:%02d (%strack#%d%s)",
                                        h, m, s, ms/10, h2, m2, s2, ms2/10,
                                        0<=extStreamId ? "ext ":"",
                                        0<=extStreamId ? extStreamId:streamId,
                                        hardsub ? ", hardsub":"");
                        }
                        else {
                            std::sprintf(msg, "Subtitle : N/A (%strack#%d%s)",
                                        0<=extStreamId ? "ext ":"",
                                        0<=extStreamId ? extStreamId:streamId,
                                        hardsub ? ", hardsub":"");
                        }
                        y += font_->DrawText(0.01f, y, font_size, font_color, msg);
                    }
                    else {
                        y += font_->DrawText(0.01f, y, font_size, font_color, "Subtitle : N/A");
                    }

                    font_->DrawText(x0-0.005f, y1 - jump1ms*4.5f, 16, Color::Yellow, "Update PBO", FONT_ALIGN_RIGHT);
                }

                font_->DrawText(x0-0.005f, y1 - jump1ms*11.12f - 0.5f*dy, 16, Color::White, "Vsync (11.11ms)", FONT_ALIGN_RIGHT);
                font_->DrawText(x0-0.005f, y1 - jump1ms*8.5f - 0.5f*dy, 16, Color::Purple, "WaitGetPoses", FONT_ALIGN_RIGHT);

                font_->DrawText(x0-0.005f, y1 - 0.9f*dy, 16, Color::Green, "Drawcalls", FONT_ALIGN_RIGHT);
                font_->DrawText(x0-0.005f, y1 - jump1ms*1.0f - 0.5f*dy, 16, Color::Blue, "VR Present", FONT_ALIGN_RIGHT);

                font_->DrawText(x0-0.005f, y1 + jump1ms*0.5f - 0.5f*dy, 16, Color::Red, "Missed Frames", FONT_ALIGN_RIGHT);
                font_->DrawText(x0-0.005f, y1 + jump1ms*2.0f - 0.5f*dy, 16, Color::White, "-2ms", FONT_ALIGN_RIGHT);
                font_->DrawText(x0-0.005f, y1 + jump1ms*3.0f - 0.5f*dy, 16, Color::Cyan, "Running Start", FONT_ALIGN_RIGHT);
            }

            renderer.EndScene();
        }
        renderer.PopState();

        if (!perf_HUD_pause_)
            cur_log_frame = (cur_log_frame+1)%FRAME_TIMING_LOG_FRAMES;

        return ret;
    }
    bool Render() {
        if (2==display_info_)
            return Render_FrameTiming();
#else
    bool Render() {
#endif
        // time metering(ugly!)
        static double pose_update_time_avg_ms = 0.0;
        static double pose_update_time = 0.0;
        static double update_texture_late = 0.0;
        static double submit_time_avg_ms = 0.0;
        static double submit_time = 0.0;
        static double last_update_time = system::GetTime();
        static int frame_count = 0;

        //
        // submit last frames - this may take 0.x ms
        //
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

        double time_begin, time_end;

        time_begin = system::GetTime();
        vrMgr_.Present(VR::HMD_EYE_LEFT, surfaceL_);
        vrMgr_.Present(VR::HMD_EYE_RIGHT, surfaceR_);
        time_end = system::GetTime();
        submit_time += (time_end - time_begin);

        time_begin = system::GetTime();
        player_.FrameMove(); // update video texture
        time_end = system::GetTime();
        if (update_texture_late<(time_end-time_begin))
            update_texture_late = (time_end-time_begin);

        // predict next pose, wait for vsync
        time_begin = system::GetTime();
        vrMgr_.UpdatePoses();
        pose_update_time += (system::GetTime() - time_begin);

        Renderer& renderer = Renderer::GetInstance();

        // design flaw...
        // must set back default renderbuffer before both present(surfaceR_)
        renderer.SetSurface(NULL);

        // 2) draw next frames to be presented in the next loop
        bool const ret = DrawScene(VR::HMD_EYE_LEFT) && DrawScene(VR::HMD_EYE_RIGHT);

        renderer.PushState();

        renderer.SetSurface(NULL);
        renderer.Clear(Color::Cyan);
        renderer.SetDepthMask(true);
        renderer.CommitChanges();
        if (renderer.BeginScene()) {
            Primitives& prim = Primitives::GetInstance();
            ITexture* tex = surfaceL_->GetRenderTarget(0);
#ifdef DRAW_SINGLE_VIEW
            if (tex) {
                if (prim.BeginDraw(tex, GFXPT_SCREEN_QUADLIST)) {
                    float const dst_aspect_ratio = renderer.GetScreenAspectRatio();
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
#else
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
#endif
            Color const viveClr = { 46, 161, 193, 255 };
            player_.DrawVideoPathOnScreen(0.005f, 0.005f, 36.0f/renderer.GetFramebufferHeight(),
                                          renderer.GetFramebufferAspectRatio(), viveClr, 0);
            float const show_info_y0 = 0.0525f;
            if (font_) {
                Color const color = { 128, 128, 128, 192 };
                int const font_size = 16;
                float y = 0.99f;
                if (display_info_>0) {
                    char msg[160];
                    float const credit_align = 0.99f - 0.26f*1440.0f/renderer.GetFramebufferWidth();
                    y -= font_->DrawText(credit_align, y, font_size, color, "    SDL, GLEW, uchardet & Balai3D", FONT_ALIGN_BOTTOM);
                    y -= font_->DrawText(credit_align, y, font_size, color, "    Google omnitone, Kiss FFT,", FONT_ALIGN_BOTTOM);
                    y -= font_->DrawText(credit_align, y, font_size, color, "    FFmpeg, NVDEC, AMD AMF,", FONT_ALIGN_BOTTOM);
                    y -= font_->DrawText(credit_align, y, font_size, color, "Powered by :", FONT_ALIGN_BOTTOM);
                    y -= 0.006f;
                    y -= font_->DrawText(credit_align, y, font_size, color, "    yumeiohya@gmail.com", FONT_ALIGN_BOTTOM);
                    y -= font_->DrawText(credit_align, y, font_size, color, "    andre.hl.chen@gmail.com", FONT_ALIGN_BOTTOM);
                    y -= font_->DrawText(credit_align, y, font_size, color, "Credits :", FONT_ALIGN_BOTTOM);
                    y -= 0.006f;
                    y -= font_->DrawText(credit_align, y, font_size, color, buildNo, FONT_ALIGN_BOTTOM);

                    y = show_info_y0;
                    Color const font_color = Color::White;
                    font_->DrawText(0.99f, 0.01f, 16, Color::Yellow, "[Toggle Display:D]", FONT_ALIGN_RIGHT);
                    std::sprintf(msg, "FPS : %d", int(GetFPS()+0.5f));
                    float const dy = font_->DrawText(0.01f, y, font_size, font_color, msg);
                    y += dy + 0.005f;

                    if (msaa_enable_) {
                        std::sprintf(msg, "MSAA : %dx", multisampleSamples_);
                        y += font_->DrawText(0.01f, y, font_size, font_color, msg);
                    }
                    else {
                        y += font_->DrawText(0.01f, y, font_size, Color::Red, "MSAA off");
                    }

                    std::sprintf(msg, "Update Poses : %.1fms %.1f%%",
                                 pose_update_time_avg_ms, pose_update_time_avg_ms*GetFPS()/10.0f);
                    y += font_->DrawText(0.01f, y, font_size, font_color, msg);

                    std::sprintf(msg, "HMD Submit: %.1fms %.1f%%",
                                 submit_time_avg_ms, submit_time_avg_ms*GetFPS()/10.0f);
                    y += font_->DrawText(0.01f, y, font_size, font_color, msg);

                    std::sprintf(msg, "Update PBO(late): %.1fms", 1000.0f*update_texture_late);
                    y += font_->DrawText(0.01f, y, font_size, font_color, msg);

                    // loop mode
                    std::sprintf(msg, "Repeat [F1] : %s", player_.LoopMode());
                    y += font_->DrawText(0.01f, y, font_size, font_color, msg);

                    if (player_.IsLoaded()) {
                        int w, h, ms, m, s;
                        std::sprintf(msg, "360 Video [F2] : %s", player_.SphericalVideoType());
                        y += font_->DrawText(0.01f, y, font_size, font_color, msg);

                        std::sprintf(msg, "Stereoscopic [F3] : %s", player_.Stereo3D());
                        y += font_->DrawText(0.01f, y, font_size, font_color, msg);

                        // video decoder info
                        if (1<player_.NumAvailableVideoDecoders()) {
                            font_->DrawText(0.01f, y, font_size, font_color, "Video Decoder [F4] :");
                            int const hw_decoder = player_.IsHardwareVideoDecoder();
                            Color const clr = (hw_decoder>0) ? ((hw_decoder>1) ? (Color::Yellow):(Color::Red)):font_color;
                            y += font_->DrawText(0.158f*1600.0f/width_, y, font_size, clr, player_.VideoDecoderName());
                        }
                        else {
                            std::sprintf(msg, "Video Decoder : %s", player_.VideoDecoderName());
                            y += font_->DrawText(0.01f, y, font_size, font_color, msg);
                        }

                        // audio decoder info
                        AUDIO_TECHNIQUE tech = AUDIO_TECHNIQUE_DEFAULT;
                        char const* audioinfo = player_.GetAudioInfo(tech);
                        if (audioinfo) {
                            if (AUDIO_TECHNIQUE_AMBIX==tech || AUDIO_TECHNIQUE_FUMA==tech) {
                                std::sprintf(msg, "Audio Decoder [F5] : %s", audioinfo);
                            }
                            else {
                                std::sprintf(msg, "Audio Decoder : %s", audioinfo);
                            }
                            y += font_->DrawText(0.01f, y, font_size, font_color, msg);
                        }

                        player_.GetVideoWidthHeight(w, h);
                        std::sprintf(msg, "Resolution : %dx%d  %.2f fps", w, h, player_.VideoFrameRate());
                        y += font_->DrawText(0.01f, y, font_size, font_color, msg);

                        // video/audio buffer
                        float const buffer_pos = 0.158f*1600.0f/width_;
                        float const packet_pos = 0.228f*1600.0f/width_;

                        int videotime = player_.VideoTime(); if (videotime<0) videotime = 0;
                        int audiotime = player_.AudioTime(); if (audiotime<0) audiotime = 0;
                        if (audioinfo && player_.IsPlaying()) {
                            int out_of_sync = (videotime-audiotime)/100;
                            if (out_of_sync>5) out_of_sync = 5;
                            else if (out_of_sync<-5) out_of_sync = -5;
                            memset(msg, '-', 11);
                            msg[out_of_sync + 5] = '+';
                            msg[11] = '\0';
                            font_->DrawText(buffer_pos, y, font_size, font_color, msg);
                        }

                        int const duration = player_.Duration();
                        if (duration>0) {
                            ms = duration%1000;
                            s  = duration/1000;
                            m  = s/60; s %= 60;
                            h  = m/60; m %= 60;
                            std::sprintf(msg, "Length : %02d:%02d:%02d:%02d", h, m, s, ms/10);
                            y += font_->DrawText(0.01f, y, font_size, font_color, msg);
                        }
                        else {
                            char const* p = "...";
                            std::sprintf(msg, "Length : live streaming%s", p + (3-((videotime/250)%4)));
                            y += font_->DrawText(0.01f, y, 15, Color::White, msg);
                        }

                        if (player_.IsPlaying()) {
                            int packets(0), sn(0);
                            int vb = player_.VideoBuffering(packets, sn);
                            if (vb>0) {
                                if (vb>10) vb = 10;
                                memset(msg, '+', vb);
                                msg[vb] = '\0';
                                font_->DrawText(buffer_pos, y, 15, font_color, msg);
                                if (packets>0) {
                                    if (packets>120) packets = 120;
                                    int c = 0;
                                    for (int i=0; i<packets; ++i) {
                                        msg[c++] = ((sn++)%100) ? '-':'+';
                                        if (0==(i+1)%10)
                                            msg[c++] = ' ';
                                    }
                                    msg[c] = '\0';
                                    font_->DrawText(packet_pos, y, 15, font_color, msg);
                                }
                            }

                            ms = videotime%1000;
                            s  = videotime/1000;
                            m  = s/60; s %= 60;
                            h  = m/60; m %= 60;
                            std::sprintf(msg, "Video : %02d:%02d:%02d:%02d", h, m, s, ms/10);
                            y += font_->DrawText(0.01f, y, font_size, font_color, msg);

                            if (audioinfo) {
                                int ab = player_.AudioBuffering(packets, sn); // millisecond
                                if (ab>0) {
                                    if (ab>800) ab = 800;
                                    ab /= 100;
                                    memset(msg, '+', ab);
                                    msg[ab] = '\0';
                                    font_->DrawText(buffer_pos, y, 15, font_color, msg);
                                    if (packets>0) {
                                        if (packets>120) packets = 120;
                                        int c = 0;
                                        for (int i=0; i<packets; ++i) {
                                            msg[c++] = ((sn++)%100) ? '-':'+';
                                            if (0==(i+1)%10)
                                                msg[c++] = ' ';
                                        }
                                        msg[c] = '\0';
                                        font_->DrawText(packet_pos, y, 15, font_color, msg);
                                    }
                                }

                                ms = audiotime%1000;
                                s  = audiotime/1000;
                                m  = s/60; s %= 60;
                                h  = m/60; m %= 60;
                                std::sprintf(msg, "Audio : %02d:%02d:%02d:%02d", h, m, s, ms/10);
                                y+=font_->DrawText(0.01f, y, font_size, font_color, msg);
                            }
                            else {
                                y+=font_->DrawText(0.01f, y, font_size, font_color, "Audio : N/A");
                            }

                            int extStreamId(0), start(0), subduration(0);
                            bool hardsub = false;
                            int const streamId = player_.SubtitleBuffering(extStreamId, hardsub, start, subduration);
                            if (streamId>=0) {
                                if (subduration>0) {
                                    ms = start;
                                    s  = ms/1000; ms %= 1000;
                                    m  = s/60; s %= 60;
                                    h  = m/60; m %= 60;
                                    int ms2 = start + subduration;
                                    int s2  = ms2/1000; ms2 %= 1000;
                                    int m2  = s2/60; s2 %= 60;
                                    int h2  = m2/60; m2 %= 60;
                                    std::sprintf(msg, "Subtitle : %02d:%02d:%02d:%02d --> %02d:%02d:%02d:%02d (%strack#%d%s)",
                                                h, m, s, ms/10, h2, m2, s2, ms2/10,
                                                0<=extStreamId ? "ext ":"",
                                                0<=extStreamId ? extStreamId:streamId,
                                                hardsub ? ", hardsub":"");
                                }
                                else {
                                    std::sprintf(msg, "Subtitle : N/A (%strack#%d%s)",
                                                0<=extStreamId ? "ext ":"",
                                                0<=extStreamId ? extStreamId:streamId,
                                                hardsub ? ", hardsub":"");
                                }
                                y += font_->DrawText(0.01f, y, 15, Color::White, msg);
                            }
                            else {
                                y+=font_->DrawText(0.01f, y, font_size, font_color, "Subtitle : N/A");
                                //y += dy;
                            }
                        }
                        else {
                            ms = player_.Timestamp(); if (ms<0) ms = 0;
                            s  = ms/1000; ms %= 1000;
                            m  = s/60; s %= 60;
                            h  = m/60; m %= 60;
                            std::sprintf(msg, "Timestamp : %02d:%02d:%02d:%02d", h, m, s, ms/10);
                            y += font_->DrawText(0.01f, y, font_size, font_color, msg);
                            y += 2.0f*dy;
                        }
                    }
                    else {
                        y += 10.0f*dy;
                    }

                    // HMD info
                    y += 0.25f*dy;
                    Matrix3 pose;
                    if (vrMgr_.GetHMDPose(pose)) {
                        std::sprintf(msg, "HMD = %.2f %.2f %.2f m", pose._14, pose._24, pose._34);
                        y += font_->DrawText(0.01f, y, font_size, Color::Green, msg);
                    }
                    else {
                        y += font_->DrawText(0.01f, y, font_size, Color::Red, "HMD lost tracking");
                    }

                    // world axes in pose frame
                    Vector3 const xAxis(pose._11, pose._12, pose._13);
                    Vector3 const yAxis(pose._21, pose._22, pose._23);
                    Vector3 const zAxis(pose._31, pose._32, pose._33);

                    // to be precise, it must be parallel projection.
                    Matrix3 xform;
                    float const fw = 7.5f;
                    float const fh = fw/renderer.GetFramebufferAspectRatio();
                    Matrix4 parallelProj;

                    renderer.SetWorldMatrix(Matrix3::Identity);
                    renderer.PushViewMatrix(gfxBuildViewMatrixFromLTM(xform, Matrix3::Identity));
                    renderer.PushProjMatrix(gfxBuildParallelProjMatrix(parallelProj, -fw, fw, -fh, fh, -2.0f, 2.0f));
                    Vector3 const origin(-0.8f*fw, 0.0f, -0.27f*fh);
                    prim.BeginDraw(NULL, GFXPT_LINELIST);
                    prim.SetColor(Color::Red);
                    prim.AddVertex(origin);
                    prim.AddVertex(origin + 0.75f*xAxis);
                    prim.SetColor(Color::Green);
                    prim.AddVertex(origin);
                    prim.AddVertex(origin + 0.75f*yAxis);
                    prim.SetColor(Color::Blue);
                    prim.AddVertex(origin);
                    prim.AddVertex(origin + 0.75f*zAxis);
                    prim.EndDraw();

                    renderer.SetBlendMode(GFXBLEND_SRCALPHA, GFXBLEND_INVSRCALPHA);
                    prim.BeginDraw(NULL, GFXPT_QUADLIST);
                        Color const front_color = { 255, 255, 255, 192 };
                        Color const back_color = { 128, 128, 128, 192 };
                        prim.SetColor(front_color);
                        Vector3 const v1 = 0.66f*(xAxis + yAxis);
                        Vector3 const v2 = 0.66f*(xAxis - yAxis);
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

                    y = 0.68f;
                    for (int i=0; i<2; ++i) {
                        VR::TrackedDevice const* device = vrMgr_.GetTrackedDevice(i);
                        if (device) {
                            if (device->IsActive()) {
                                char const* focusing = (device==player_.GetFocusController()) ? "*":" ";
                                if (device->IsTracked()) {
                                    Vector3 const pos = device->GetPose().Origin();
                                    std::sprintf(msg, "Controller #%d%s %.2f %.2f %.2f m", i, focusing, pos.x, pos.y, pos.z);
                                    y += font_->DrawText(0.01f, y, font_size, Color::Green, msg);
                                }
                                else {
                                    std::sprintf(msg, "Controller #%d%s lost tracking", i, focusing);
                                    y += font_->DrawText(0.01f, y, font_size, Color::Yellow, msg);
                                }

                                VR::ControllerState s, s0;
                                device->GetControllerState(s, &s0);

                                if (s.Menu) {
                                    y += font_->DrawText(0.01f, y, font_size, Color::White, "     Menu : On");
                                }
                                else {
                                    y += font_->DrawText(0.01f, y, font_size, Color::Gray, "     Menu : Off");
                                }

                                if (s.OnTouchpad) {
                                    std::sprintf(msg, "     Touchpad : %s", (s.OnTouchpad>1) ? "Press":"Touch");
                                    y += font_->DrawText(0.01f, y, font_size, Color::White, msg);
                                }
                                else {
                                    y += font_->DrawText(0.01f, y, font_size, Color::Gray, "     Touchpad : Off");
                                }
#if 0
                                // test touchpad swiping...
                                if (s.OnTouchPad && s0.OnTouchPad) {
                                    font_->DrawText(0.01f, y, font_size, Color::White, "       Speed:");
                                    VR::ControllerState::Axis a(s.TouchPad);
                                    a -= s0.TouchPad;
                                    float speed = sqrtf(a.x*a.x + a.y*a.y)*GetFPS(); // instant speed
                                    float const align = 0.09f;
                                    prim.BeginDraw(NULL, GFXPT_SCREEN_LINELIST);
                                        speed /= 20.0f;
                                        prim.SetColor(Color::Red);
                                        prim.AddVertex2D(align, y);
                                        prim.AddVertex2D(align, y+dy);
                                        prim.AddVertex2D(align, y+0.5f*dy);
                                        prim.AddVertex2D(align+speed, y+0.5f*dy);
                                        prim.SetColor(Color::White);
                                        for (int i=0; i<10; ++i) {
                                            float x = align + 0.05f*(i+1);
                                            prim.AddVertex2D(x, y);
                                            prim.AddVertex2D(x, y+dy);
                                        }
                                    prim.EndDraw();
                                }
                                y += dy;
#endif
                                if (s.Grip) {
                                    y += font_->DrawText(0.01f, y, font_size, Color::White, "     Grip : On");
                                }
                                else {
                                    y += font_->DrawText(0.01f, y, font_size, Color::Gray, "     Grip : Off");
                                }

                                if (s.Trigger>0.0f) {
                                    std::sprintf(msg, "     Trigger : %.4f", s.Trigger);
                                    y += font_->DrawText(0.01f, y, font_size, Color::White, msg);
                                }
                                else {
                                    y += font_->DrawText(0.01f, y, font_size, Color::Gray, "     Trigger : Off");
                                }
                            }
                            else {
                                std::sprintf(msg, "Controller #%d inactive", i);
                                y += font_->DrawText(0.01f, y, font_size, Color::Gray, msg);
                            }
                            y += 0.02f; // alittle more spacing
                        }
                    }
                }
                else {
                    font_->DrawText(0.99f, y, font_size, color, buildNo, FONT_ALIGN_RIGHT_BOTTOM);
                }
            }
//#endif
            renderer.EndScene();
        }
        renderer.PopState();

        // status
        time_begin = system::GetTime();
        ++frame_count;
        if ((time_begin-last_update_time)>1.0f) {
            pose_update_time_avg_ms = 1000.0 * pose_update_time/frame_count;
            submit_time_avg_ms = 1000.0 * submit_time/frame_count;
            frame_count = 0;
            submit_time = pose_update_time = 0.0;
            last_update_time = time_begin;
            update_texture_late = 0.0;
        }

        return ret;
    }
    void Cleanup() {
        player_.Finalize();
        vrMgr_.Finalize();
        BL_SAFE_RELEASE(font_);
        BL_SAFE_RELEASE(surfaceL_);
        BL_SAFE_RELEASE(surfaceR_);
    }

    bool SDLEventHandler(SDL_Event const& event) {
        if (SDL_KEYDOWN==event.type) {
            SDL_KeyboardEvent const& key = event.key;
            switch (key.keysym.sym)
            {
#ifdef OPTIMIZING_SHOW_FRAME_TIMIING
            case SDLK_e:
                if (0==key.repeat)
                    perf_HUD_pause_ = !perf_HUD_pause_;
                break;

            case SDLK_d:
                if (0==key.repeat)
                    display_info_ = (display_info_+1)%3;
                break;
#else
            case SDLK_d:
                if (0==key.repeat)
                    display_info_ = (display_info_+1)%2;
                break;
#endif

#ifdef BL_DEBUG_BUILD
            case SDLK_m:
                if (0==key.repeat)
                    msaa_enable_ = (msaa_enable_) ? 2:3;
                break;
#endif
            case SDLK_F1:
                player_.ToggleLoopMode();
                break;

            case SDLK_F2: // video type : plane, 180, 360
                if (0==key.repeat)
                    player_.ToggleSphericalVideoType();
                break;

            case SDLK_F3: // mono, stereo,...
                if (0==key.repeat) {
                    player_.ToggleStereoScope();
                }
                break;

            case SDLK_F4: // SW <--> HW Video decoder
                if (0==key.repeat)
                    player_.ToggleHardwareVideoDecoder();
                break;

            case SDLK_F5: // ambiX <--> FuMa
                if (0==key.repeat)
                    player_.ToggleAmbisonicFormat();
                break;

#ifndef HTC_VIVEPORT_RELEASE
            case SDLK_F10:
                if (0==key.repeat)
                    player_.ToggleAudioChange();
                break;

            case SDLK_F11:
                if (0==key.repeat)
                    player_.ToggleSubtitleChange();
                break;
#endif
            case SDLK_KP_MINUS:
            case SDLK_MINUS:
                player_.PlaySeek(-15000);
                break;

            case SDLK_KP_PLUS:
            case SDLK_PLUS:
                player_.PlaySeek(15000);
                break;

            case SDLK_UP:
                if (0==key.repeat)
                    player_.Replay();
                break;

            case SDLK_DOWN:
                if (0==key.repeat)
                    player_.PlayAtEnd();
                break;

            case SDLK_LEFT:
                if (0==key.repeat)
                    player_.PrevTrack();
                return true;

            case SDLK_RIGHT:
                if (0==key.repeat)
                    player_.NextTrack();
                return true;

            case SDLK_SPACE:
                if (0==key.repeat)
                    player_.ToggleStartStop();
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