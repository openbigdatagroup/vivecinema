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
 * @file    VRVideoPlayer.cpp
 * @author  andre chen
 * @history 2015/12/09 created
 *
 */
#include "VRVideoPlayer.h"
#include "SystemFont.h"
#include "BLQuaternion.h"
#include "BLPrimitives.h"

#include "HWAccelDecoder.h"

#ifndef HTC_VIVEPORT_RELEASE
#include "BLJPEG.h" // load background 360 texture
#include "BLPNG.h" // load UI
//#define TEST_OUTPUT_FONT_TEXTURE
#endif

#include "BLFileStream.h"

using namespace mlabs::balai;
using namespace mlabs::balai::system;
using namespace mlabs::balai::math;
using namespace mlabs::balai::graphics;

// size of texture borad height(in meters) when viewing in 1 meter away(cf. text_xform_)
static float const best_text_size_view_in_1m = 0.1f;//0.078f; // 0.072m = 7.2cm

// show current track name we just played
static float const show_new_video_filename_period = 5.0f;

// dashboard, menu viewing distance, and size...
static float const dashboard_default_distance = 12.5f; // modify this if menu too small(far)
static float const dashboard_hit_distance_min =  2.0f;
static float const dashboard_hit_distance_max = 90.0f;
static float const dashboard_default_height   = 10.0f;
static float const dashboard_default_width    = dashboard_default_height * 1920.0f/1080.0f;

static float const dashboard_default_widget_width  = 0.925f*dashboard_default_width;
static float const dashboard_default_widget_height = 0.175f*dashboard_default_height;
static float const dashboard_default_widget_left = -0.5f*dashboard_default_widget_width;
static float const dashboard_default_widget_top  = dashboard_default_widget_height - 0.5f*dashboard_default_height;

#ifndef HTC_VIVEPORT_RELEASE

// legendary AVI non compressed format may be slow, if it's too big (e.g. 4K)...
#define SUPPORT_AVI

#endif

namespace htc {

mlabs::balai::graphics::Color const VRVideoPlayer::viveColor_ = { 46, 161, 193, 255 };

float const VRVideoPlayer::menu_animation_duration_ = 0.75f;
float const VRVideoPlayer::animation_duration_ = 0.25f;

float PickTest(float& hit_x, float& hit_z,
               mlabs::balai::math::Matrix3 const& xform,
               mlabs::balai::VR::TrackedDevice const* controller)
{
    if (NULL!=controller && controller->IsTracked()) {
        Vector3 const normal = xform.YAxis();
        Vector3 hit, dir;
        controller->GetPointer(hit, dir);
        float dist = -normal.Dot(hit-=xform.Origin());
        float step = normal.Dot(dir);
        if (dist*step>0.0f && fabs(step)>1.e-4f) {
            dist /= step;
            hit += dist * dir; // hit point

            hit_x = hit.Dot(xform.XAxis());
            hit_z = hit.Dot(xform.ZAxis());

            return dist;
        }
    }
    return -1.0f;
}

//---------------------------------------------------------------------------------------
VideoTrack::VideoTrack():
sa3d_(),
texcoord_(),
type_(VIDEO_TYPE_2D),
type_intrinsic_(VIDEO_TYPE_2D),
source_(VIDEO_SOURCE_UNKNOWN),
name_(NULL),
fullpath_(NULL),
fullpath_bytes_(0),
thumbnail_texture_id_(-1),
font_texture_id_(-1), // invalid
width_(0),
height_(0),
spherical_longitude_(0),
spherical_latitude_south_(0),
spherical_latitude_north_(0),
thumbnail_cache_(0),
timeout_(250),
duration_(0),
duration_end_(0),
timestamp_(0),
subtitle_stream_id_(0), // not specified
audio_stream_id_(0),non_diegetic_audio_stream_id_(0xff),
status_(0), // 0:N/A, 1:ready, 2:error
full3D_(0),
liveStream_(false),
spatialAudioWithNonDiegetic_(false)
{
}
//---------------------------------------------------------------------------------------
VideoTrack::~VideoTrack()
{
    blFree(name_);
    fullpath_ = name_ = NULL;

    fullpath_bytes_ = 0;

    thumbnail_texture_id_ = font_texture_id_ = -1;
    type_ = type_intrinsic_ = VIDEO_TYPE_2D;
    status_ = full3D_ = 0;
    subtitle_stream_id_ = audio_stream_id_ = 0xff;
}

//---------------------------------------------------------------------------------------
bool VRVideoPlayer::SphereGeometry::Create(int longitude, int latitude_south, int latitude_north)
{
    if (longitude_==longitude && latitude_south==latitude_south_ && latitude_north==latitude_north_ && vao_) {
        return true;
    }

    if (0<longitude && longitude<=360 && -90<=latitude_south && latitude_south<latitude_north && latitude_north<=90) {
        num_indices_ = 0;
        longitude_ = latitude_south_ = latitude_north_ = 0;

        if (0==vao_) glGenVertexArrays(1, &vao_);
        if (0==vbo_) glGenBuffers(1, &vbo_);
        if (0==ibo_) glGenBuffers(1, &ibo_);

        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);

        int const vb_stride = 3*sizeof(float);
        int const num_longs = (longitude>=360) ? NUM_LONGITUDES:(NUM_LONGITUDES+1);
        int vertexCnt = num_longs * (NUM_LATITUDES - 1);
        if (90==latitude_north)
            ++vertexCnt;
        else
            vertexCnt += num_longs;

        if (-90==latitude_south)
            ++vertexCnt;
        else
            vertexCnt += num_longs;

        int const vb_size = vertexCnt*vb_stride;
        int const reserve_indices = 2*(NUM_LONGITUDES+1)*NUM_LATITUDES;
        int const ib_reserve_size = reserve_indices*sizeof(uint16);
        void* buffer = malloc(vb_size>ib_reserve_size ? vb_size:ib_reserve_size);
        {
            float* vb = (float*) buffer;
            float const dV = float(3.14159265359*(latitude_north-latitude_south)/(180*NUM_LATITUDES));
            float const dH = float(6.28318530718*longitude/(360*NUM_LONGITUDES));
            float const theta0 = float(3.14159265359*(90+longitude/2)/180.0);
            float phi0 = 0.0f;

            if (latitude_north>=90) { // north pole
                *vb++ = 0.0f;
                *vb++ = 0.0f;
                *vb++ = 1.0f;
            }
            else {
                phi0 = float((90-latitude_north)*3.14159265359/180.0);
                float const cos_phi = cos(phi0);
                float const sin_phi = sin(phi0);
                for (int j=0; j<num_longs; ++j) {
                    float const theta = theta0 - j*dH;
                    *vb++ = sin_phi * cos(theta);
                    *vb++ = sin_phi * sin(theta);
                    *vb++ = cos_phi;
                }
            }

            for (int i=1; i<NUM_LATITUDES; ++i) {
                float const phi = phi0 + i*dV;
                float const cos_phi = cos(phi);
                float const sin_phi = sin(phi);
                for (int j=0; j<num_longs; ++j) {
                    float const theta = theta0 - j*dH;
                    *vb++ = sin_phi * cos(theta);
                    *vb++ = sin_phi * sin(theta);
                    *vb++ = cos_phi;
                }
            }

            if (latitude_south<=-90) { // south pole
                *vb++ =  0.0f;
                *vb++ =  0.0f;
                *vb++ = -1.0f;
            }
            else {
                float const phi = float((90-latitude_south)*3.14159265359/180.0);
                float const cos_phi = cos(phi);
                float const sin_phi = sin(phi);
                for (int j=0; j<num_longs; ++j) {
                    float const theta = theta0 - j*dH;
                    *vb++ = sin_phi * cos(theta);
                    *vb++ = sin_phi * sin(theta);
                    *vb++ = cos_phi;
                }
            }
            assert((vb-(float*)buffer)==3*vertexCnt);
        }
        glBufferData(GL_ARRAY_BUFFER, vb_size, buffer, GL_STATIC_DRAW);

        // fill ib
        uint16* ib = (uint16*) buffer;
        uint16 id;
        if (longitude>=360) {    
            if (latitude_north>=90) { // north pole
                for (int j=0; j<NUM_LONGITUDES; ++j) {
                    *ib++ = 0;
                    *ib++ = (uint16) (j + 1);
                }
                *ib++ = 0;
                *ib++ = 1;
                for (int i=2; i<NUM_LATITUDES; ++i) {
                    if (i&1) { // forward
                        uint16 const id0 = id = (uint16) ((i-2)*NUM_LONGITUDES + 1);
                        for (int j=0; j<NUM_LONGITUDES; ++j,++id) {
                            *ib++ = id;
                            *ib++ = (uint16) (id + NUM_LONGITUDES);
                        }
                        *ib++ = id0;
                        *ib++ = id0 + NUM_LONGITUDES;
                    }
                    else { // reverse
                        *ib++ = id = (uint16) ((i-1)*NUM_LONGITUDES + 1); --id;
                        for (int j=1; j<NUM_LONGITUDES; ++j,--id) {
                            *ib++ = id;
                            *ib++ = (uint16) (id + NUM_LONGITUDES);
                        }
                        *ib++ = id;
                    }
                }
            }
            else {
                for (int i=1; i<NUM_LATITUDES; ++i) {
                    if (i&1) { // forward
                        uint16 const id0 = id = (uint16) ((i-1)*NUM_LONGITUDES);
                        for (int j=0; j<NUM_LONGITUDES; ++j,++id) {
                            *ib++ = id;
                            *ib++ = (uint16) (id + NUM_LONGITUDES);
                        }
                        *ib++ = id0;
                        *ib++ = id0 + NUM_LONGITUDES;
                    }
                    else { // reverse
                        *ib++ = id = (uint16) (i*NUM_LONGITUDES); --id;
                        for (int j=1; j<NUM_LONGITUDES; ++j,--id) {
                            *ib++ = id;
                            *ib++ = (uint16) (id + NUM_LONGITUDES);
                        }
                        *ib++ = id;
                    }
                }
            }

            // south
            if (latitude_south<=-90) {
                uint16 const s_pole_id = uint16(vertexCnt - 1);
                if (NUM_LATITUDES&1) { // forward
                    uint16 const id0 = id = s_pole_id - NUM_LONGITUDES;
                    for (int j=0; j<NUM_LONGITUDES; ++j) {
                        *ib++ = id++;
                        *ib++ = s_pole_id;
                    }
                    *ib++ = id0;
                    //*ib++ = s_pole_id; // degenerated tri
                }
                else { // reverse
                    *ib++ = id = s_pole_id; --id;
                    for (int j=1; j<NUM_LONGITUDES; ++j) {
                        *ib++ = id--;
                        *ib++ = s_pole_id;
                    }
                    *ib++ = id;
                }
            }
            else {
                if (NUM_LATITUDES&1) {
                    uint16 const id0 = id = (uint16) (vertexCnt - 2*NUM_LONGITUDES);
                    for (int j=0; j<NUM_LONGITUDES; ++j,++id) {
                        *ib++ = id;
                        *ib++ = uint16(id+NUM_LONGITUDES);
                    }
                    *ib++ = id0;
                    *ib++ = id0 + NUM_LONGITUDES;
                }
                else {
                    *ib++ = id = (uint16) (vertexCnt - NUM_LONGITUDES); --id;
                    for (int j=0; j<NUM_LONGITUDES; ++j,--id) {
                        *ib++ = id;
                        *ib++ = id + NUM_LONGITUDES;
                    }
                }
            }
        }
        else { // longitude<360
            if (latitude_north>=90) { // north pole
                for (int j=0; j<num_longs; ++j) {
                    *ib++ = 0;
                    *ib++ = (uint16) (j + 1);
                }

                for (int i=2; i<NUM_LATITUDES; ++i) {
                    if (i&1) { // forward
                        id = (uint16) ((i-2)*num_longs + 1);
                        for (int j=0; j<num_longs; ++j,++id) {
                            *ib++ = id;
                            *ib++ = (uint16) (id + num_longs);
                        }
                    }
                    else { // reverse
                        *ib++ = id = (uint16) (i*num_longs);
                        id -= uint16(num_longs+1);
                        for (int j=2; j<num_longs; ++j,--id) {
                            *ib++ = id;
                            *ib++ = (uint16) (id + num_longs);
                        }
                        *ib++ = id;
                    }
                }
            }
            else {
                for (int i=1; i<NUM_LATITUDES; ++i) {
                    if (i&1) { // forward
                        id = (uint16) ((i-1)*num_longs);
                        for (int j=0; j<num_longs; ++j,++id) {
                            *ib++ = id;
                            *ib++ = (uint16) (id + num_longs);
                        }
                    }
                    else { // reverse
                        *ib++ = id = (uint16) ((i+1)*num_longs - 1);
                        id -= uint16(num_longs+1);
                        for (int j=2; j<num_longs; ++j,--id) {
                            *ib++ = id;
                            *ib++ = (uint16) (id + num_longs);
                        }
                        *ib++ = id;
                    }
                }
            }

            // south
            if (latitude_south<=-90) {
                uint16 const s_pole_id = uint16(vertexCnt - 1);
                if (NUM_LATITUDES&1) { // forward
                    id = uint16(s_pole_id - num_longs);
                    for (int j=1; j<num_longs; ++j) {
                        *ib++ = id++;
                        *ib++ = s_pole_id;
                    }
                    *ib++ = id;
                }
                else { // reverse
                    *ib++ = id = s_pole_id; --id;
                    for (int j=2; j<num_longs; ++j) {
                        *ib++ = --id;
                        *ib++ = s_pole_id;
                    }
                    *ib++ = --id;
                }
            }
            else {
                if (NUM_LATITUDES&1) {
                    id = (uint16) (vertexCnt - 2*num_longs);
                    for (int j=0; j<num_longs; ++j,++id) {
                        *ib++ = id;
                        *ib++ = uint16(id+num_longs);
                    }
                }
                else {
                    *ib++ = id = (uint16) (vertexCnt - 1);
                    id -= uint16(num_longs+1);
                    for (int j=1; j<num_longs; ++j,--id) {
                        *ib++ = id;
                        *ib++ = (uint16) (id + num_longs);
                    }
                }
            }
        }

        num_indices_ = (GLsizei)(ib - (uint16*) buffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, num_indices_*sizeof(uint16), buffer, GL_STATIC_DRAW);
        free(buffer);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vb_stride, (GLvoid const*)0);

        // unbind all
        glBindVertexArray(0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        longitude_ = longitude;
        latitude_south_ = latitude_south;
        latitude_north_ = latitude_north;

        return true;
    }
    return false;
}

//---------------------------------------------------------------------------------------
VRVideoPlayer::VRVideoPlayer():
thumbnailThread_(),
pageCV_(),thumbnailCV_(),
thumbnailMutex_(),
publishingThumbnailId_(-1),
thumbnail_filecache_(NULL),
decoder_(this, 0),
videoTexture_(),
audioManager_(AudioManager::GetInstance()),
masterVolume_(),
fonts_(16),
playList_(1024),current_track_(NULL),
extSubtitleInfo_(NULL),
on_processing_(0),
on_processing_time_start_(0.0f),
fontFx_(NULL),
subtitleFx_(NULL),subtitleFxCoeff_(NULL),subtitleFxShadowColor_(NULL),
pan360_(NULL),pan360Crop_(NULL),pan360Diffuse_(NULL),pan360Map_(NULL),
pan360NV12_(NULL),pan360NV12Crop_(NULL),pan360MapY_(NULL),pan360MapUV_(NULL),
videoNV12_(NULL),videoMapY_(NULL),videoMapUV_(NULL),
subtitle_(NULL),
uiGlyph_(NULL),
background_(NULL),
vrMgr_(mlabs::balai::VR::Manager::GetInstance()),
focus_controller_(NULL),
thumbnailBuffer_(NULL),audioBuffer_(NULL),
thumbnailBufferSize_(0),audioBufferSize_(0),
fullsphere_(),customized_(),
hmd_xform_(Matrix3::Identity),
viewer_xform_(Matrix3::Identity),
dashboard_xform_(Matrix3::Identity),
text_xform_(Matrix3::Identity),
dashboard_distance_(0.0f),
dashboard_width_(dashboard_default_width),
dashboard_height_(dashboard_default_height),
dashboard_hit_x_(0.0f),
dashboard_hit_z_(0.0f),
dashboard_hit_dist_(100.0f),
//screen_width_(dashboard_default_height),
//screen_height_(dashboard_default_height),
widget_left_(dashboard_default_widget_left),
widget_top_(dashboard_default_widget_top),
widget_width_(dashboard_default_widget_width),
widget_height_(dashboard_default_widget_height),
azimuth_adjust_(0.0f),
subtitle_rect_count_(0),subtitle_pts_(0),subtitle_duration_(0),
event_timestamp_(0.0f),
subevent_timestamp_(0.0f),
widget_timer_(0.0f),
menu_page_scroll_(0.0f),
menu_page_(-1),
menu_picking_slot_(-1),
quit_count_down_(-1),
dim_bkg_(0),
total_subtitle_streams_(0),
total_audio_streams_(0),
menu_auto_scroll_(0),
trigger_dashboard_(0),
trigger_dashboard_align_(0),
widget_on_(0),
loop_mode_(0)
{
    // hook interrupt
    vrMgr_.InterruptHandler() = [this] (VR::VR_INTERRUPT e, void* d) { VRInterrupt_(e, d); };

    video_path_[0] = 0;
    for (int i=0; i<MENU_VIDEO_THUMBNAIL_TEXTURES; ++i) {
        videoThumbnails_[i] = NULL;
        reference_tracks_[i] = -1;
    }

    memset(texcoords_, 0, sizeof(texcoords_));
    memset(extSubtitleFilename_, 0, sizeof(extSubtitleFilename_));

    for (int i=0; i<MAX_LANGUAGE_STREAMS; ++i) {
        subtitles_[i] = languages_[i] = ISO_639_UNKNOWN;
        subStmIDs_[i] = lanStmIDs_[i] = -1;
    }

    for (int i=0; i<Subtitle::MAX_NUM_SUBTITLE_RECTS; ++i) {
        subtitle_rects_[i].Reset();
    }

    // thumbnail buffer
    // size should be at least 256 bytes + thumbnail texture size;
    // VIDEO_THUMBNAIL_TEXTURE_WIDTHxVIDEO_THUMBNAIL_TEXTURE_HEIGHT(RGB)
    thumbnailBufferSize_ = 2*VIDEO_THUMBNAIL_TEXTURE_WIDTH*VIDEO_THUMBNAIL_TEXTURE_HEIGHT*3;
    thumbnailBuffer_ = (uint8*) blMalloc(thumbnailBufferSize_);
    if (NULL==thumbnailBuffer_) {
        thumbnailBufferSize_ = 0;
    }

    // audio buffer
    audioBufferSize_ = 4096*18*sizeof(float); // 3rd order ambisonics + headlock
    audioBuffer_ = (uint8*) blMalloc(audioBufferSize_);
    if (NULL==audioBuffer_) {
        audioBufferSize_ = 0;
    }
}
//---------------------------------------------------------------------------------------
VRVideoPlayer::~VRVideoPlayer()
{
    blFree(thumbnailBuffer_); thumbnailBuffer_ = NULL; thumbnailBufferSize_ = 0;
    blFree(audioBuffer_); audioBuffer_ = NULL; audioBufferSize_ = 0;
    current_track_ = NULL;
    playList_.for_each(safe_delete_functor());
    playList_.clear();
}
//---------------------------------------------------------------------------------------
bool VRVideoPlayer::Playback_(VideoTrack* track)
{
    if (NULL==track)
        return false;

    total_subtitle_streams_ = total_audio_streams_ = 0;
    subtitle_rect_count_ = subtitle_pts_ = subtitle_duration_ = 0;

    if (NULL!=current_track_ && 0==(0x1000&on_processing_)) {
        current_track_->SetTimestamp(decoder_.Timestamp());
    }

    // clear black
    videoTexture_.ClearBlack();

#if 1
    on_processing_time_start_ = (float) mlabs::balai::system::GetTime();
    on_processing_ = 0x1000;

    //
    // watch out for the std::async()...
    // the temporary object (of type std::future<>) results from std::async() call will
    // be destructed soon after std::async() returned. According to C++ standard,
    // std::future<> destructor will block std::this_thread until task finished.
    // Hence makes concurrency not as we expected.
    //
    // std::this_thread is our render thread. if it blocks too long, steam VR interrupts
    // our rendering. (the result is  a short pause as user may experience)
    //
    // so if you're building vive cinema using later visual studios version, like 2015,
    // 2017... use std::thread() here not std::async().
    //
    // For Fossil visual studio 2012, std::async() does work as i wish. -andre 2017.12.25
    //
    std::async(std::launch::async, [this,track] {
        VideoOpenOption param;
        track->GetVideoParams(param);
        param.IsLiveStream = track->IsLiveStream();
        if (decoder_.Open(track->GetFullPath(), &param)) {
            int audio_channels[MAX_LANGUAGE_STREAMS];
            total_subtitle_streams_ = decoder_.GetSubtitleStreamInfo(subStmIDs_, subtitles_, MAX_LANGUAGE_STREAMS);
            total_audio_streams_ = decoder_.GetAudioStreamInfo(lanStmIDs_, audio_channels, languages_, MAX_LANGUAGE_STREAMS);
            track->SetSubtitleStreamID(decoder_.SubtitleStreamID());
            track->SetAudioStreamID(decoder_.AudioStreamID());
            int const video_width = decoder_.VideoWidth();
            int const video_height = decoder_.VideoHeight();
            track->SetSize(video_width, video_height);

            // 2016.09.12 duration may be wrong!!!
            // 2016.12.30 exclude live streaming case(duration=-1.0f)
            int const duration = decoder_.GetDuration();
            if (0!=param.IsLiveStream && duration>0) {
                int const play_duration = track->SetDuration(duration);
                if ((play_duration+5000)<duration) {
                    decoder_.FixErrorDuration(play_duration);
                }
            }

            current_track_ = track;
            assert(0x1000==on_processing_);
            on_processing_ = 0x1002; // succeed
        }
        else {
            track->SetFail();
            if (NULL!=current_track_) {
                current_track_ = track; // issued by Next Track
            }
            assert(0x1000==on_processing_);
            on_processing_ = 0x1001; // failed
        }
    });
    return true;

#else
    // 
    // load video single thread... it's stalled!!!
    // (for reference only... to be removed...)
    //
    on_processing_ = 0;
    VideoOpenOption param;
    track->GetStreamIDs(param.SubtitleStreamIndex, param.AudioStreamIndex);
    track->GetHRTF(param.SpatialAudio);
    param.IsLiveStream = track->IsLiveStream();
    if (decoder_.Open(track->GetFullPath(), &param)) {
        total_subtitle_streams_ = decoder_.GetSubtitleStreamInfo(subStmIDs_, subtitles_, MAX_LANGUAGE_STREAMS);
        total_audio_streams_ = decoder_.GetAudioStreamInfo(lanStmIDs_, languages_, MAX_LANGUAGE_STREAMS);
        track->SetSubtitleStreamID(decoder_.SubtitleStreamID());
        track->SetAudioStreamID(decoder_.AudioStreamID());
        int const video_width = decoder_.VideoWidth();
        int const video_height = decoder_.VideoHeight();
        track->SetSize(video_width, video_height);

        // 2016.09.12 duration may be wrong!!!
        // 2016.12.30 exclude live streaming case(duration=-1.0f)
        int const duration = decoder_.GetDuration();
        if (0!=param.IsLiveStream && duration>0) {
            int const play_duration = track->SetDuration(duration);
            if ((play_duration+5000)<duration) {
                decoder_.SetDuration(play_duration);
            }
        }

        current_track_ = track;
        decoder_.Play();

        return true;
    }

    return false;
#endif
}
//---------------------------------------------------------------------------------------
bool VRVideoPlayer::AlignSubtitle360Transform_(mlabs::balai::math::Matrix3& xform) const
{
    Matrix3 const& pose = hmd_xform_;
    Vector3 const Z(0.0f, 0.0f, 1.0f);
    Vector3 Y = pose.YAxis();
    Vector3 X = Y.Cross(Z);
    float len = 0.0f;
    X.Normalize(&len);
    if (len>0.25f) {
        Y = Z.Cross(X);
    }
    else { // Y is up/down
        Y = Z.Cross(pose.XAxis());
        Y.Normalize();
        X = Y.Cross(Z);
    }

    // X
    xform._11 = X.x;
    xform._21 = X.y;
    xform._31 = X.z;

    // Y
    xform._12 = Y.x;
    xform._22 = Y.y;
    xform._32 = Y.z;

    // Z
    xform._13 = Z.x;
    xform._23 = Z.y;
    xform._33 = Z.z;

    // put 1 meter away in front
    xform.SetOrigin(pose.Origin() + Y);

    return true;
}
//---------------------------------------------------------------------------------------
bool VRVideoPlayer::ResetDashboardPose_()
{
    Matrix3 pose;
    if (vrMgr_.GetHMDPose(pose)) {
        Vector3 const Z(0.0f, 0.0f, 1.0f);
        Vector3 Y = pose.YAxis();
        Vector3 X = Y.Cross(Z);
        float len = 0.0f;
        X.Normalize(&len);
        if (len>0.25f) {
            Y = Z.Cross(X);
        }
        else { // Y is up/down
            Y = Z.Cross(pose.XAxis());
            Y.Normalize();
            X = Y.Cross(Z);
        }

        // X
        dashboard_xform_._11 = X.x;
        dashboard_xform_._21 = X.y;
        dashboard_xform_._31 = X.z;

        // Y
        dashboard_xform_._12 = Y.x;
        dashboard_xform_._22 = Y.y;
        dashboard_xform_._32 = Y.z;

        // Z
        dashboard_xform_._13 = Z.x;
        dashboard_xform_._23 = Z.y;
        dashboard_xform_._33 = Z.z;

        // to do... fit screen size!?
        dashboard_distance_ = dashboard_default_distance;
        Y = Y + pose.YAxis(); Y.Normalize();
        dashboard_xform_.SetOrigin(pose.Origin() + dashboard_distance_*Y);
        if (dashboard_xform_._34<1.2f)
            dashboard_xform_._34 = 1.2f; // low viewing angle isn't comfortable...

        text_xform_ = dashboard_xform_;
        Y = 7.0f*Y + pose.YAxis(); Y.Normalize(); // trick again, more centered
        text_xform_.SetOrigin(pose.Origin() + Y); // 1 meter away front

        return true;
    }
    else {
        dashboard_distance_ = dashboard_default_distance;
        text_xform_ = dashboard_xform_.MakeIdentity();
        dashboard_xform_.SetOrigin(0.0f, dashboard_distance_, 0.0f);
        text_xform_.SetOrigin(0.0f, 1.0f, 0.0f);

        return false;
    }
}
//---------------------------------------------------------------------------------------
void VRVideoPlayer::ConnectFocusController_()
{
    if (NULL!=focus_controller_ && !focus_controller_->IsActive()) {
        focus_controller_ = NULL;
    }

    for (int i=0; i<2; ++i) {
        VR::TrackedDevice const* device = vrMgr_.GetTrackedDevice(i);
        if (device!=focus_controller_ && NULL!=device && device->IsActive()) {
            if (NULL==focus_controller_) {
                focus_controller_ = device;
                Rumble_VRController(focus_controller_);
            }
            else if (NULL==current_track_) { // resolve dual - menu mode
                if (0<=menu_picking_slot_ ||
                    0.0f<=DashboardPickVideoTrack_(focus_controller_)) {
                }
                else if (!focus_controller_->GetAttention()) {
                    if (device->GetAttention() ||
                        (device->IsTracked() && !focus_controller_->IsTracked()) ||
                        (0.0f<=DashboardPickVideoTrack_(device) && 0.0f>DashboardPickVideoTrack_(focus_controller_)) ||
                        (device->IsKeyOn() && !focus_controller_->IsKeyOn())) {
                        focus_controller_ = device;
                        Rumble_VRController(focus_controller_);
                    }
                }
            }
            else if (widget_on_>1) { // resolve due - playback mode, menu opened
                if (!focus_controller_->GetAttention()) {
                    if (device->GetAttention() || (device->IsTracked() && !focus_controller_->IsTracked())) {
                        focus_controller_ = device;
                        Rumble_VRController(focus_controller_);
                    }
                    else {
                        float hit_x1, hit_z1, hit_x2, hit_z2;
                        if (DashboardPickTest_(hit_x2, hit_z2, device)>0.0f) {
                            if ((DashboardPickTest_(hit_x1, hit_z1, focus_controller_)<=0.0f) || 
                                (hit_z2<widget_top_ && hit_z1>widget_top_ && 17!=widget_on_ && 15!=widget_on_)){
                                focus_controller_ = device;
                                Rumble_VRController(focus_controller_);
                                return;
                            }
                        }
                    }
                }
            }
            else { // resolve due - playback mode, menu closed
                if (!focus_controller_->GetAttention()) {
                    if (device->GetAttention() || 
                        (device->IsTracked() && !focus_controller_->IsTracked()) ||
                        (device->IsKeyOn() && !focus_controller_->IsKeyOn()) ) {
                        focus_controller_ = device;
                        Rumble_VRController(focus_controller_);
                    }
                    else {
                        float hit_x1, hit_z1, hit_x2, hit_z2;
                        if (DashboardPickTest_(hit_x2, hit_z2, device)>0.0f &&
                            DashboardPickTest_(hit_x1, hit_z1, focus_controller_)<=0.0f) {
                            focus_controller_ = device;
                            Rumble_VRController(focus_controller_);
                        }
                    }
                }
            }

            return; // since we consider only 2 controllers
        }
    }
}
//---------------------------------------------------------------------------------------
void VRVideoPlayer::RealignDashboard_(mlabs::balai::VR::TrackedDevice const* ctrl)
{
    assert(NULL!=ctrl && dashboard_hit_dist_>0.0f);
    if (trigger_dashboard_align_) {
        float const duration = 5.0f;
        float const alpha = ((float) mlabs::balai::system::GetTime()-subevent_timestamp_)/duration;
        Matrix3 pose;
        if (0.0f<=alpha && alpha<=1.0f && vrMgr_.GetHMDPose(pose)) {
            math::Vector3 const los = pose.YAxis();
            if (1&trigger_dashboard_align_ && -0.9f<los.z && los.z<0.9f) {
                float const to = atan2(los.x, los.y);
                math::Vector3 const front = dashboard_xform_.YAxis();
                if (-0.8f<front.z && front.z<0.8f) {
                    // rotate angle, take the shortest cut
                    float angle = atan2(front.x, front.y) - to;
                    if (angle>math::constants::float_pi) {
                        angle -= math::constants::float_two_pi;
                    }
                    else if (angle<-math::constants::float_pi) {
                        angle += math::constants::float_two_pi;
                    }

                    if (fabs(angle)>2.0f*math::constants::float_deg_to_rad) { // done within 2 degrees
                        //
                        // it looks like no-brainer since no rotation 'pivot' is considered.
                        // but in the end, the dashboard_xform_ position will be
                        // will be justified. see the last 3 lines
                        // dashboard_xform_.SetOrigin(from + ....) below.
                        Matrix3 rot(Matrix3::Identity);
                        rot.SetEulerAngles(0.0f, 0.0f, alpha*angle);
                        //dashboard_xform_ *= rot;
                        dashboard_xform_ = rot*dashboard_xform_;
                    }
                    else {
                        trigger_dashboard_align_ &= ~1;
                    }
                }
                else {
                    trigger_dashboard_align_ &= ~1;
                }
            }
#if 0
            if (2&trigger_dashboard_align_) {
                math::Vector3 const front = dashboard_xform_.YAxis();
                float const cos_angle = front.Dot(los);
                // arc cos(0.1f) ~= 84.26 degrees
                if (cos_angle>0.1f || cos_angle<-0.1f) {
                    float to = acos(los.z);

                    if (fabs(to-math::constants::float_half_pi)<7.5f*math::constants::float_deg_to_rad)
                        to = math::constants::float_half_pi;

                    // opposite facing if cos_angle<0.0f...
                    float pitch = acos((cos_angle>0.0f) ? front.z:-front.z) - to;
                    if (fabs(pitch)>2.0f*math::constants::float_deg_to_rad) { // done within 2 degrees
                        Matrix3 rot(Matrix3::Identity);
                        pitch *= ((cos_angle>0.0f) ? alpha:-alpha);
                        rot.SetEulerAngles(pitch, 0.0f, 0.0f);
                        dashboard_xform_ *= rot;
                    }
                    else {
                        trigger_dashboard_align_ &= ~2;
                    }
                }
                else {
                    trigger_dashboard_align_ &= ~2;
                }
            }
#endif
        }
        else {
            trigger_dashboard_align_ = 0;
        }
    }

    //
    // the "pinpoint" = from + dashboard_hit_dist_ * dir
    // and we want to adjust coordnate system's origin, so that the "grab point" will
    // locate at local point (dashboard_hit_x_, 0, dashboard_hit_z_)
    Vector3 from, dir;
    ctrl->GetPointer(from, dir);
    dashboard_xform_.SetOrigin(from + dashboard_hit_dist_ * dir -
                               dashboard_hit_x_*dashboard_xform_.XAxis() -
                               dashboard_hit_z_*dashboard_xform_.ZAxis());
}
//---------------------------------------------------------------------------------------
bool VRVideoPlayer::Initialize()
{
    // hw context
    mlabs::balai::video::hwaccel::InitContext(NULL);

    // init audio
    AudioConfig configs[4];
    configs[0].Format = AUDIO_FORMAT_F32;
    configs[0].Channels = 2;
    configs[0].SampleRate = 48000;

    configs[1].Format = AUDIO_FORMAT_F32;
    configs[1].Channels = 2;
    configs[1].SampleRate = 44100;

    configs[2].Format = AUDIO_FORMAT_S16;
    configs[2].Channels = 2;
    configs[2].SampleRate = 48000;

    configs[3].Format = AUDIO_FORMAT_S16;
    configs[3].Channels = 2;
    configs[3].SampleRate = 44100;

    if (!audioManager_.InitAudio([this](uint8* data, int size, AudioConfig const& config) {
                                    RenderAudioData_(data, size, config);
                                 },
                                 configs, 4)) {
        BL_ERR("VRVideoPlayer::Initialize() init audio failed... no audio!!!\n");
    }

    //
    //
    // andre : geometry respect to Balai coordinate system(Z is up)...
    //
    //

    // vertex shader
    char const* vp = "#version 410 core\n"
        "uniform mat4 matViewProj;\n"
        "layout(location=0) in vec4 pos;\n"
        "out vec3 t3;\n"
        "void main() {\n"
        "  gl_Position = pos*matViewProj;\n"
        "  t3 = pos.xyz;\n"
        "}";

    // fragment shader
    // caution : atan(0, 0) is undefined
    char const* pan_360_fp = "#version 410 core\n"
        "uniform sampler2D map;\n"
        "uniform highp vec4 cropf;\n"
        "uniform lowp vec4 diffuse;\n"
        "in vec3 t3;\n"
        "layout(location=0) out vec4 c0;\n"
        "void main() {\n"
        "  highp vec3 t = normalize(t3);\n"
        "  t.x = cropf.x + cropf.y*atan(t.x, t.y);\n"
        "  t.y = cropf.z + cropf.w*acos(t.z);\n"
        "  c0 = diffuse*texture(map, t.xy);\n"
        "}";

    uint32 program = CreateGLProgram(vp, pan_360_fp);
    GLProgram* glShader = new GLProgram(0);
    if (glShader->Init(program)) {
        pan360_ = glShader;
    }
    else {
        return false;
    }
    pan360Crop_ = pan360_->FindConstant("cropf");
    pan360Diffuse_ = pan360_->FindConstant("diffuse");
    pan360Map_ = pan360_->FindSampler("map");

    //
    // 2017.08.25 Change color conversion for Tsai Min-Liang's Deserted @ Venice Film Festival.
    //  Rec.709 color space with normalize Y' to [16, 235]  U/V:[16, 240] with Umax=.436 Vmax=.615
    //
    // video_360_fp and video_fp
    //
    char const* video_360_fp = "#version 410 core\n"
        "uniform sampler2D mapY;\n"
        "uniform sampler2D mapUV;\n"
        "uniform highp vec4 cropf;\n"
        "in vec3 t3;\n"
        "layout(location=0) out vec4 c0;\n"
        "void main() {"
        "  highp vec3 t = normalize(t3);"
        "  t.x = cropf.x + cropf.y*atan(t.x, t.y);"
        "  t.y = cropf.z + cropf.w*acos(t.z);"
        "  mediump vec3 nv12;"
        "  nv12.x = clamp(1.16438*texture(mapY, t.xy).r - 0.07306, 0.0, 1.0);"
        "  nv12.yz = clamp((texture(mapUV, t.xy).rg - vec2(0.5, 0.5))*vec2(1.00689, 1.40091), vec2(-0.436, -0.615), vec2(0.436, 0.615));"
        "  c0.xyz = mat3(1.00000,  1.00000, 1.00000,"
        "                0.00000, -0.21482, 2.12798,"
        "                1.28033, -0.38059, 0.00000)*nv12;"
        "  c0.w = 1.0;"
        "}";
    program = CreateGLProgram(vp, video_360_fp);
    glShader = new GLProgram(0);
    if (glShader->Init(program)) {
        pan360NV12_ = glShader;
    }
    else {
        return false;
    }
    pan360NV12Crop_ = pan360NV12_->FindConstant("cropf");
    pan360MapY_ = pan360NV12_->FindSampler("mapY");
    pan360MapUV_ = pan360NV12_->FindSampler("mapUV");

    // common vertex shader (for Primiive)
    char const* vsh_v_c_tc1 = "#version 410 core\n"
        "layout(location=0) in vec4 position;\n"
        "layout(location=1) in mediump vec4 color;\n"
        "layout(location=2) in vec2 texcoord;\n"
        "uniform mat4 matWorldViewProj;\n"
        "out vec4 color_;\n"
        "out vec2 texcoord_;\n"
        "void main() {\n"
        "  gl_Position = position * matWorldViewProj; \n"
        "  color_ = color;\n"
        "  texcoord_ = texcoord;\n"
    "}";

    // video shader - YUV to RGB(BT.709)
    char const* video_fp = "#version 410 core\n"
        "in lowp vec4 color_;\n"
        "in mediump vec2 texcoord_;\n"
        "uniform sampler2D mapY;\n"
        "uniform sampler2D mapUV;\n"
        "layout(location=0) out vec4 c0;\n"
        "void main() {\n"
        "  mediump vec3 nv12; \n"
        "  nv12.x = clamp(1.16438*texture(mapY, texcoord_.xy).r - 0.07306, 0.0, 1.0);\n"
        "  nv12.yz = clamp((texture(mapUV, texcoord_.xy).rg - vec2(0.5, 0.5))*vec2(1.00689, 1.40091), vec2(-0.436, -0.615), vec2(0.436, 0.615));\n"
        "  c0.xyz = mat3(1.00000,  1.00000, 1.00000,\n"
        "                0.00000, -0.21482, 2.12798,\n"
        "                1.28033, -0.38059, 0.00000)*nv12;\n"
        "  c0.w = 1.0;\n"
        "}";

    program = CreateGLProgram(vsh_v_c_tc1, video_fp);
    glShader = new GLProgram(0);
    if (glShader->Init(program)) {
        videoNV12_ = glShader;
    }
    else {
        return false;
    }
    videoMapY_ = videoNV12_->FindSampler("mapY");
    videoMapUV_ = videoNV12_->FindSampler("mapUV");

    // font rendering
#if 0
    char const* psh_vr = "#version 410 core\n"
        "in lowp vec4 color_;\n"
        "in mediump vec2 texcoord_;\n"
        "uniform highp vec4 coeff;\n"
        "uniform sampler2D diffuseMap;\n"
        "out vec4 output;\n"
        "void main() {\n"
        " float dist = texture2D(diffuseMap, texcoord_).r;\n"
        " if (dist<coeff.x) discard;\n"
        " vec2 s = smoothstep(coeff.xy, coeff.zw, vec2(dist, dist));\n"
        " output.rgb = mix(vec3(0,0,0), color_.rgb, s.y);\n"
        " s = s*s;\n"
        " output.a = color_.a*mix(0.2*s.x, 0.2+0.8*s.y, s.y);\n"
        "}";
#endif

    //
    // This is a simple (alpha-kill) font rendering.
    // 1) discard(pixel kill) is not necessary for most cases. But It must have
    //    if zwrite is on.
    // 2) Currently, font distance map is built of dispread factor=16, i.e.
    //    each step for a pixel is 1.0/16 = 0.0625 downgrade when leaving.
    //    this and following shader constants are based on that setting.
    //    0.25 = 4 pixel steps, 0.5 = 8 pixel step.(The text boundary is where alpha=0.5)
    // 3) Regard 0.3 and 0.7 be 2 magic numbers. (Do NOT change that)
    // 4) This is the simplest font rendering shader with minimal aliasing.
    //    (flicking in VR).
    //
    // Caution : smoothstep(edge0, edge1, x) results are undefined if edge0>=edge1!
    //
    //
    char const* psh_font = "#version 410 core\n"
        "in lowp vec4 color_;\n"
        "in mediump vec2 texcoord_;\n"
        "uniform sampler2D diffuseMap;\n"
        "layout(location=0) out vec4 c0;\n"
        "void main() {\n"
        " float dist = texture2D(diffuseMap, texcoord_).r;\n"
        " if (dist<0.25) discard;\n"
        " vec2 s = smoothstep(vec2(0.25, 0.45), vec2(0.45, 0.55), vec2(dist, dist));\n"
        " c0.rgb = color_.rgb;\n"
        " s = s*s;\n"
        " c0.a = color_.a*mix(0.3*s.x, 0.3+0.7*s.y, s.y);\n"
    "}";
    program = CreateGLProgram(vsh_v_c_tc1, psh_font);
    glShader = new GLProgram(0);
    if (glShader->Init(program)) {
        fontFx_ = glShader;
    }
    else {
        return false;
    }

    //
    // font shader - outline and shadow
    //
#if 0
    //
    // [DO NOT REMOVE]
    //
    // attempt #1 A straight forward implementation.
    // It surely work but result is noticely saw-toothed alaising
    //
    // coeff.x = outline cutoff, e.g. 0.25
    // coeff.y = shadow cutoff, e.g. 0.3 
    // coeff.zw = shadow texcoord offset
    char const* psh_outline_shadow = "#version 410 core\n"
        "in lowp vec4 color_;\n"
        "in mediump vec2 texcoord_;\n"
        "uniform highp vec4 coeff;\n"
        "uniform sampler2D diffuseMap;\n"
        "out vec4 outputColor;\n"
        "void main() {\n"
        " float alpha = texture2D(diffuseMap, texcoord_).r;\n"
        " if (alpha<coeff.x) {\n"
        "   alpha = texture2D(diffuseMap, texcoord_+coeff.zw).r;\n"
        "   if (alpha<coeff.y) discard;\n"
        "   outputColor.rgb = vec3(0.1, 0.1, 0.1);\n"
        "   outputColor.a = color_.a*smoothstep(0.2, 0.5, alpha);\n"
        " }\n"
        " else {\n"
        "   outputColor.rgb = mix(vec3(0,0,0), color_.rgb, smoothstep(0.5, 0.55, alpha));\n"
        "   outputColor.a = color_.a;\n"
        " }\n"
        "}";

    //
    // attempt #2 improve attempt #1 by work around branching.
    // It works better than attempt #1. Saw-toothed alaising is eased, But VR kind
    // alaising, flicking, is spotable. This shader is actually very good if text is
    // big enough. When you draw near, this effect is quite good.
    // (ver 0.5.214, 5th submit)
    char const* psh_outline_shadow =
        "#version 410 core\n"
        "in lowp vec4 color_;\n"
        "in mediump vec2 texcoord_;\n"
        "uniform highp vec4 coeff;\n"
        "uniform sampler2D diffuseMap;\n"
        "out vec4 result;\n"
        "void main() {\n"
        " vec4 coeff2 = vec4(0.05, 0.2, 0.45, 0.55);\n"
        " vec2 v2 = smoothstep(coeff2.xz, coeff2.yw, texture2D(diffuseMap, texcoord_).rr);\n"
        " result.rgb = mix(vec3(0,0,0), color_.rgb, v2.y);\n"
        " result.a = color_.a*(v2.x+0.8*smoothstep(coeff2.x, coeff2.y, texture2D(diffuseMap, texcoord_+coeff.zw).r));\n"
        "}";
#endif

    //
    // attempt #3 To avoid flicking effect, this version use 2nd pass for
    // rendering outline and shadow. The main drawback is, if you draw near,
    // it looks more blurring than attempt #2.
    //
    // Note : this is the 2nd pass only, need fontFx_ effect to render main text
    //        before applying this effect.
    //
    // ver 0.6.254+
    //
    // 2016.12.01 remove "s = s*s;\n"
    //
    //  ......
    //  " clr.a = shadow.a*s.z*smoothstep(0.1, 0.5, texture2D(diffuseMap, texcoord_+coeff.xy).r);\n"
    //  " s = s*s;\n"
    //  " highp float border = color_.a*mix(0.3*s.w, 0.3+0.7*s.z, s.z)*mix(0.3*s.x, 0.3+0.7*s.y, s.y);\n"
    //  ......
    //
    char const* psh_outline_shadow =
        "#version 410 core\n"
        "in lowp vec4 color_;\n"
        "in mediump vec2 texcoord_;\n"
        "uniform highp vec4 coeff;\n"
        "uniform highp vec4 shadow;\n"
        "uniform sampler2D diffuseMap;\n"
        "layout(location=0) out vec4 c0;\n"
        "void main() {\n"
        " highp vec4 s = smoothstep(vec4(0.0, 0.2, 0.45, 0.55), vec4(0.2, 0.3, 0.55, 0.75), texture2D(diffuseMap, texcoord_).rrrr);\n"
        " s.zw = vec2(1.0, 1.0) - s.zw;\n"
        " highp vec4 clr, clr2;\n"
        " clr.rgb = shadow.rgb;\n"
        " clr.a = shadow.a*s.z*smoothstep(0.1, 0.5, texture2D(diffuseMap, texcoord_+coeff.xy).r);\n"
        " highp float border = color_.a*mix(0.3*s.w, 0.3+0.7*s.z, s.z)*mix(0.3*s.x, 0.3+0.7*s.y, s.y);\n"
        " clr2.rgb = color_.rgb;\n"
        " clr2.a = border;\n"
        " c0 = mix(clr, clr2, border);\n"
        "}";
    /////////////////////////////////////////////////////////////////////////////////////
    //
    // TO-DO : attempt #4, and mipmapping!?
    //
    /////////////////////////////////////////////////////////////////////////////////////
    program = CreateGLProgram(vsh_v_c_tc1, psh_outline_shadow);
    glShader = new GLProgram(0);
    if (glShader->Init(program)) {
        subtitleFx_ = glShader;
    }
    else {
        return false;
    }
    subtitleFxCoeff_ = subtitleFx_->FindConstant("coeff");
    subtitleFxShadowColor_ = subtitleFx_->FindConstant("shadow");

    // geometry
    fullsphere_.Create(360);
    customized_.Create(180);

    extSubtitleInfo_ = Texture2D::New(0, true);
    extSubtitleInfo_->SetAddressMode(ADDRESS_CLAMP, ADDRESS_CLAMP);
    extSubtitleInfo_->SetFilterMode(FILTER_BILINEAR);

    // video texture, a pixel buffer object texture
    videoTexture_.Initialize();

    // subtitle
    subtitle_ = Texture2D::New(0, true);
    subtitle_->SetAddressMode(ADDRESS_CLAMP, ADDRESS_CLAMP);
    subtitle_->SetFilterMode(FILTER_BILINEAR);

    // load assets
    int width, height, channels;
    fileio::ifstream fin;
    if (fin.Open("./assets/asset.bin")) {
        fileio::FileHeader header;
        fileio::FileChunk chunk;
        fin >> header;
        if (0==memcmp(header.Description , "Vive Cinema 360", 15)) {
            uint32 const draw_glyph = (('D'<<24)|('R'<<16)|('A'<<8)|('W'));
            uint32 const pan360 = (('P'<<24)|('3'<<16)|('6'<<8)|('0'));
            int dummy;
            do {
                fin >> chunk;
                switch (chunk.ID)
                {
                case draw_glyph:
                    if (NULL==uiGlyph_) {
                        uint32 const pos0 = fin.GetPosition();
                        fin >> width >> height >> channels >> dummy;
                        for (int i=DRAW_UI_MENU; i<DRAW_GLYPH_TOTALS; ++i) {
                            TexCoord& tc = texcoords_[i];
                            fin >> tc.x0 >> tc.y0 >> tc.x1 >> tc.y1;
                        }

                        int const data_size = height*width*channels;
                        uint8* pixels = (uint8*) blMalloc(data_size);
                        fin.Read(pixels, data_size);
                        uiGlyph_ = Texture2D::New(0, false);
                        if (uiGlyph_) {
                            uiGlyph_->SetAddressMode(ADDRESS_CLAMP, ADDRESS_CLAMP);
                            uiGlyph_->SetFilterMode(FILTER_BILINEAR);
                            uiGlyph_->UpdateImage((uint16)width, (uint16)height, FORMAT_RGBA8, pixels);
                        }
                        blFree(pixels);

                        uint32 const data_pad = chunk.ChunkSize() - (fin.GetPosition() - pos0);
                        if (data_pad>0) {
                            fin.SeekCur(data_pad);
                        }
                    }
                    else {
                        fin.SeekCur(chunk.ChunkSize());
                    }
                    break;

                case pan360:
                    if (NULL==background_) {
                        uint32 const pos0 = fin.GetPosition();
                        fin >> width >> height >> channels >> dummy;
                        assert(channels==3);
                        int const data_size = height*width*channels;
                        uint8* pixels = (uint8*) blMalloc(data_size);
                        fin.Read(pixels, data_size);
                        Texture2D* tex = Texture2D::New(0);
                        tex->UpdateImage((uint16)width, (uint16) height, FORMAT_RGB8, pixels);
                        background_ = tex;
                        blFree(pixels);

                        uint32 const data_pad = chunk.ChunkSize() - (fin.GetPosition() - pos0);
                        if (data_pad>0) {
                            fin.SeekCur(data_pad);
                        }
                    }
                    else {
                        fin.SeekCur(chunk.ChunkSize());
                    }
                    break;

                default:
                    fin.SeekCur(chunk.ChunkSize());
                    break;
                }

                if (NULL!=uiGlyph_ && NULL!=background_)
                    break;

            } while (!fin.Fail());
        }
    }
#ifndef HTC_VIVEPORT_RELEASE
    else {
        time_t lt = time(NULL);
        tm const* now = localtime(&lt);

        fileio::ofstream stream("./assets/asset.bin");
        assert(!stream.Fail());

        fileio::FileHeader fileHeader;
        fileHeader.Platform = fileio::FileHeader::PLATFORM_WIN32;
        fileHeader.Version  = 0;
        fileHeader.Size     = 0; // TBD
        fileHeader.Date[0] = (uint8) (now->tm_year%100);
        fileHeader.Date[1] = (uint8) (now->tm_mon%12 + 1);
        fileHeader.Date[2] = (uint8) now->tm_mday;
        fileHeader.Date[3] = (uint8) now->tm_hour;
        strcpy(fileHeader.Description, "Vive Cinema 360");
        stream.BeginFile(fileHeader);

        // Icon
        fileio::FileChunk fc;
        fc.ID = (('I'<<24)|('C'<<16)|('O'<<8)|('N'));
        fc.Version  = 0;
        fc.Elements = 1;
        fc.Size     = 0;
        strcpy(fc.Description, "App Icon RGB565");
        stream.BeginChunk(fc);
        void* icon = mlabs::balai::image::read_JPEG("./assets/vive_64.jpg", width, height, channels);
        if (icon) {
            uint16* dst = (uint16*) icon;
            uint8 const* src = (uint8 const*) icon;
            int const total_pixels = width*height;
            for (int i=0; i<total_pixels; ++i,src+=channels) {
                *dst++ = ((src[0]&0xf8)<<8) | ((src[1]&0xfc)<<3) | ((src[2]&0xf8)>>3);
            }
            stream << width << height << channels << (uint32) 0;
            stream.WriteBytes(icon, 2*total_pixels);
            free(icon);
        }
        stream.EndChunk(fc);

        // ui texture
        uint16 const ui_width = 1024;
        uint16 const ui_height = 1024;
        int const ui_stride = ui_width * 4;
        int const ui_data_size = ui_height*ui_stride;
        uint8* pixels = (uint8*) blMalloc(ui_data_size);
        memset(pixels, 0, ui_data_size);
        char const* ui_sources[] = {
            "./assets/backMenu.png",  // DRAW_UI_MENU
            "./assets/circle.png",    // DRAW_UI_CIRCLE

                // 2016.08.30
                "./assets/light.png",    // DRAW_UI_LIGHT
                "./assets/audio.png",    // DRAW_UI_LANGUAGE
                "./assets/subtitle.png", // DRAW_UI_SUBTITLE
                // 2016.09.10
                "./assets/busy.png",     // DRAW_UI_BUSY

            "./assets/nextTrack.png", // DRAW_UI_NEXT
            NULL,                     // DRAW_UI_PREV, mirrow of DRAW_UI_NEXT
            "./assets/playIcon.png",  // DRAW_UI_PLAY
            "./assets/pauseIcon.png", // DRAW_UI_PAUSE
            "./assets/repeat.png",    // DRAW_UI_REPLAY
            "./assets/volume.png",    // DRAW_UI_VOLUME
            "./assets/mute.png",      // DRAW_UI_MUTE
            "./assets/settings.png",  // DRAW_UI_SETTINGS
            "./assets/switch_base.png",   // DRAW_UI_SWITCH_BASE
            "./assets/switch_button.png", // DRAW_UI_SWITCH_BUTTON

            NULL, // DRAW_UI_CONFIG_RECT
            NULL, // DRAW_UI_SWITCH_3D
            NULL, // DRAW_UI_SWITCH_3D_SBS
            NULL, // DRAW_UI_SWITCH_3D_TB
            NULL, // DRAW_UI_SWITCH_VR
            NULL, // DRAW_UI_SWITCH_VR_ANGLE

            NULL, // DRAW_UI_VOLUME_BAR
            NULL, // DRAW_UI_SEEK_BAR, no texture yet
        };

        int x_pos(0), y_pos(0), y_next(0);
        int const x_spacing = 4;
        int const y_spacing = 4;
        for (int i=DRAW_UI_MENU; i<DRAW_GLYPH_TOTALS; ++i) {
            TexCoord& tc = texcoords_[i];
            if (DRAW_UI_PREV==i) {
                TexCoord const& next = texcoords_[DRAW_UI_NEXT];
                tc.x0 = next.x1;
                tc.x1 = next.x0;
                tc.y0 = next.y0;
                tc.y1 = next.y1;
                continue;
            }
            tc.x0 = tc.x1 = tc.y0 = tc.y1 = 0.0f;
            char const* png = ui_sources[i-DRAW_UI_MENU];
            uint8* image = (uint8*) mlabs::balai::image::read_PNG(png, width, height, channels);
            assert((NULL==png) ^ (NULL!=image));
            if (NULL!=image) {
                assert(channels==4);
                if (x_pos+width>ui_width) {
                    x_pos = 0;
                    y_pos += (y_next+y_spacing);
                }

                if ((x_pos+width)<=ui_width && (y_pos+height)<=ui_height) {
                    tc.x0 = float(x_pos)/float(ui_width);
                    tc.y0 = float(y_pos)/float(ui_height);
                    tc.x1 = float(x_pos+width)/float(ui_height);
                    tc.y1 = float(y_pos+height)/float(ui_height);

                    uint8* dst = pixels + y_pos*ui_stride + x_pos*4;
                    uint8 const* src = image;
                    int const src_stride = width*channels;
                    for (int i=0; i<height; ++i,dst+=ui_stride,src+=src_stride) {
                        memcpy(dst, src, src_stride);
                    }

                    x_pos += (width+x_spacing);
                    if (y_next<height)
                        y_next = height;
                }
                else {
                    BL_ERR("Failed to cook UIGlyph texture... make ui_width/ui_height bigger! 2048?\n");
                }

                free(image);
            }
        }

        mlabs::balai::image::write_PNG("./assets/ui_glyph_debug_check.png", pixels, ui_width, ui_height, 4, 8);

        uiGlyph_ = Texture2D::New(0, false);
        if (uiGlyph_) {
            uiGlyph_->SetAddressMode(ADDRESS_CLAMP, ADDRESS_CLAMP);
            uiGlyph_->SetFilterMode(FILTER_BILINEAR);
            uiGlyph_->UpdateImage(ui_width, ui_height, FORMAT_RGBA8, pixels);
        }

        fc.ID = (('D'<<24)|('R'<<16)|('A'<<8)|('W'));
        fc.Version  = 0;
        fc.Elements = 0;
        fc.Size     = 0;
        strcpy(fc.Description, "UI Widget Glyph");
        stream.BeginChunk(fc);
        stream << (int) ui_width << (int) ui_height << (int) 4 << (int) 32;
        for (int i=DRAW_UI_MENU; i<DRAW_GLYPH_TOTALS; ++i) {
            TexCoord const& tc = texcoords_[i];
            stream << tc.x0 << tc.y0 << tc.x1 << tc.y1;
            ++fc.Elements;
        }
        stream.WriteBytes(pixels, ((int)ui_width*(int)ui_height*4));
        stream.EndChunk(fc);
        blFree(pixels);

        // environment pan360 texture
        fc.ID = (('P'<<24)|('3'<<16)|('6'<<8)|('0'));
        fc.Version  = 0;
        fc.Elements = 1;
        fc.Size     = 0;
        strcpy(fc.Description, "Environment 360");
        stream.BeginChunk(fc);
        uint8* pan360 = (uint8*) mlabs::balai::image::read_JPEG("./assets/viveNight_resize.jpg", width, height, channels);
        stream << (int) width << height << channels << (int) 0;
        stream.WriteBytes(pan360, width*height*channels);
        stream.EndChunk(fc);

        BL_ASSERT(width==4096 && height==2048 && channels==3);
        Texture2D* tex = Texture2D::New(0);
        tex->UpdateImage((uint16)width, (uint16) height, FORMAT_RGB8, pan360);
        background_ = tex;
        free(pan360);

        stream.EndFile();
    }
#endif

    return true;
}
//---------------------------------------------------------------------------------------
void VRVideoPlayer::Finalize()
{
    // save timestamp
    if (NULL!=current_track_ && 0==(0x1000&on_processing_)) {
        current_track_->SetTimestamp(decoder_.Timestamp());
    }

    if (thumbnailThread_.joinable()) {
        menu_page_ = -1; // stop sign
        pageCV_.notify_all();
        thumbnailCV_.notify_all();
        thumbnailThread_.join();
    }

    decoder_.Close(); // good to be here!?

    // relase video texture
    videoTexture_.Finalize();
    
    // hw context
    mlabs::balai::video::hwaccel::DeinitContext();

    subtitleFxCoeff_ = subtitleFxShadowColor_ = NULL;
    pan360Crop_ = pan360Diffuse_ = pan360NV12Crop_ = NULL;
    pan360Map_ = pan360MapY_ = pan360MapUV_ = NULL;
    videoMapY_ = videoMapUV_ = NULL;
    BL_SAFE_RELEASE(fontFx_);
    BL_SAFE_RELEASE(subtitleFx_);
    BL_SAFE_RELEASE(pan360_);
    BL_SAFE_RELEASE(pan360NV12_);
    BL_SAFE_RELEASE(videoNV12_);
    BL_SAFE_RELEASE(subtitle_);
    BL_SAFE_RELEASE(uiGlyph_);
    BL_SAFE_RELEASE(background_);
    BL_SAFE_RELEASE(extSubtitleInfo_);
    audioManager_.DeInitAudio();

    if (NULL!=thumbnail_filecache_) {
        // save tweakable data
        int const total_videos = playList_.size();
        uint8 user_tweak[8]; // { is_360, s3D intrinsic, s3D tweak }
        for (int i=0; i<total_videos; ++i) {
            VideoTrack* video = playList_[i];
            if (video->IsValid()) {
                int const offset = video->ThumbnailCache();
                if (offset>0 && 0==fseek(thumbnail_filecache_, offset+8, SEEK_SET)) {
                    user_tweak[0] = (uint8) video->Type();
                    user_tweak[1] = (uint8) video->Type_Intrinsic();
                    int longi, lati1, lati2;
                    if (video->GetSphericalAngles(longi, lati1, lati2) && longi<=360) {
                        user_tweak[2] = (uint8) (longi/6);
                    }
                    else {
                        user_tweak[2] = 0;
                    }

                    assert(offset+8==ftell(thumbnail_filecache_));
                    fwrite(user_tweak, 1, 3, thumbnail_filecache_);
                    assert(offset+11==ftell(thumbnail_filecache_));
                    if (0==fseek(thumbnail_filecache_, offset+250, SEEK_SET)) {
                        assert(offset+250==ftell(thumbnail_filecache_));
                        int const timestamp = video->GetTimestamp();
                        user_tweak[0] = video->Audio();
                        user_tweak[1] = video->Subtitle();
                        user_tweak[2] = (uint8) ((timestamp&0xff000000)>>24);
                        user_tweak[3] = (uint8) ((timestamp&0x00ff0000)>>16);
                        user_tweak[4] = (uint8) ((timestamp&0x0000ff00)>>8);
                        user_tweak[5] = (uint8) (timestamp&0x000000ff);
                        fwrite(user_tweak, 1, 6, thumbnail_filecache_);
                    }
                }
                else {
                    //BL_ERR("failed to seek!?\n");
                }
            }
        }
        fclose(thumbnail_filecache_);
        thumbnail_filecache_ = NULL;
    }

    // tracks
    current_track_ = NULL;
    playList_.for_each(safe_delete_functor());
    playList_.clear();

    for (int i=0; i<MENU_VIDEO_THUMBNAIL_TEXTURES; ++i) {
        BL_SAFE_RELEASE(videoThumbnails_[i]);
        reference_tracks_[i] = -1;
    }

    // font
    fonts_.for_each(safe_release_functor());
    fonts_.clear();

    // geometry
    fullsphere_.Destroy();
    customized_.Destroy();
}
//---------------------------------------------------------------------------------------
inline size_t GetVideoFullFilename(wchar_t* wfilename, int max_length,
                                   wchar_t const* path, wchar_t const* fileName)
{
    size_t short_len = wcslen(fileName);
    if (short_len>3) {
        bool accept = false;
        if (short_len>4 && fileName[short_len-4]==L'.') {
            wchar_t const* ext = fileName + short_len - 3;
            if (0==memcmp(ext, L"mp4", 3*sizeof(wchar_t)) || 0==memcmp(ext, L"MP4", 3*sizeof(wchar_t)) ||
                0==memcmp(ext, L"mov", 3*sizeof(wchar_t)) || 0==memcmp(ext, L"MOV", 3*sizeof(wchar_t)) ||
                0==memcmp(ext, L"mts", 3*sizeof(wchar_t)) || 0==memcmp(ext, L"MTS", 3*sizeof(wchar_t)) ||
                0==memcmp(ext, L"vob", 3*sizeof(wchar_t)) || 0==memcmp(ext, L"VOB", 3*sizeof(wchar_t)) ||
                0==memcmp(ext, L"wmv", 3*sizeof(wchar_t)) || 0==memcmp(ext, L"WMV", 3*sizeof(wchar_t)) ||
#ifdef SUPPORT_AVI
                0==memcmp(ext, L"avi", 3*sizeof(wchar_t)) || 0==memcmp(ext, L"AVI", 3*sizeof(wchar_t)) ||
#endif
                0==memcmp(ext, L"mkv", 3*sizeof(wchar_t)) || 0==memcmp(ext, L"MKV", 3*sizeof(wchar_t))) {
                accept = true;
            }
        }
        else if (short_len>5 && fileName[short_len-5]==L'.') {
            wchar_t const* ext = fileName + short_len - 4;
            if (0==memcmp(ext, L"m2ts", 4*sizeof(wchar_t)) || 0==memcmp(ext, L"M2TS", 4*sizeof(wchar_t)) ||
                0==memcmp(ext, L"webm", 4*sizeof(wchar_t)) || 0==memcmp(ext, L"WEBM", 4*sizeof(wchar_t)) ||
                0==memcmp(ext, L"divx", 4*sizeof(wchar_t)) || 0==memcmp(ext, L"DIVX", 4*sizeof(wchar_t))) {
                accept = true;
            }
        }
        else if (fileName[short_len-3]==L'.') {
            wchar_t const* ext = fileName + short_len - 2;
            if (0==memcmp(ext, L"ts", 2*sizeof(wchar_t)) || 0==memcmp(ext, L"TS", 2*sizeof(wchar_t))) {
                accept = true;
            }
        }

        if (accept)
            return swprintf(wfilename, max_length, L"%s/%s", path, fileName);
    }

    return 0;
}
int VRVideoPlayer::SetMediaPath(wchar_t const* path)
{
    // tracks
    current_track_ = NULL;
    playList_.for_each(safe_delete_functor());
    playList_.clear();
    playList_.reserve(1024);

    // fonts
    fonts_.for_each(safe_release_functor());
    fonts_.clear();

    menu_page_scroll_ = 0.0f;
    menu_page_ = 0;
    menu_picking_slot_ = -1;
    menu_auto_scroll_ = 0;
    trigger_dashboard_ = 0;
    widget_on_ = 0;

    for (int i=0; i<MENU_VIDEO_THUMBNAIL_TEXTURES; ++i) {
        reference_tracks_[i] = -1;
    }

    if (NULL==path)
        return 0;

    // font to draw video (short) file name
    TextBlitter fontBlitter;
    char const* font_name = "Calibri"; // to make Korean shown properly
    ISO_639 lan = ISO_639_ENG;
    if (!fontBlitter.Create(font_name, UI_TEXT_FONT_SIZE, true, lan, UI_TEXT_FONT_DISTANCE_SPREAD_FACTOR)) {
        return 0;
    }

    // set environment's default locale
    // On program startup, the locale selected is the minimal "C" locale.
    // this will affect behavior of (wide) character function like wcstombs
    //char const* oldlocale = "C"; // setlocale(LC_CTYPE, NULL);
    //setlocale(LC_CTYPE, "");

    wchar_t wfullpath[256];
    wchar_t wfilename[256];

    // DRAW_TEXT - be cautious!!!
    char const* text_out[DRAW_TEXT_TOTALS] = {
        NULL, NULL,   // placeholder
        "Oops... No Videos!", // DRAW_TEXT_NO_VIDEOS
        "App Will Quit.", // DRAW_TEXT_APP_WILL_QUIT
        "Viewer Discretion Is Advised.", // DRAW_TEXT_WARNING
        "Failed to Play!", // DRAW_TEXT_ERROR
        "Menu",  // DRAW_TEXT_MENU
        "Exit",  // DRAW_TEXT_EXIT
        "Next:", // DRAW_TEXT_NEXT

        "3D",    // DRAW_TEXT_3D
        "SBS",   // DRAW_TEXT_LR
        "TB",    // DRAW_TEXT_TB

        "VR",    // DRAW_TEXT_VR
        "360",   // DRAW_TEXT_360
        "180",   // DRAW_TEXT_180

        // 2016.08.31
        "Track#", // DRAW_TEXT_TRACK_NO,
        "Subtitle Off", // DRAW_TEXT_SUBTITLE_DISABLE

        "720P",  // DRAW_TEXT_720P
        "1080P", // DRAW_TEXT_1080P
        "4K",    // DRAW_TEXT_4K
        "8K",    // DRAW_TEXT_8K

        ":", // DRAW_TEXT_COLON
        "/", // DRAW_TEXT_SLASH
        "0", // DRAW_TEXT_0
        "1", // DRAW_TEXT_1
        "2", // DRAW_TEXT_2
        "3", // DRAW_TEXT_3
        "4", // DRAW_TEXT_4
        "5", // DRAW_TEXT_5
        "6", // DRAW_TEXT_6
        "7", // DRAW_TEXT_7
        "8", // DRAW_TEXT_8
        "9", // DRAW_TEXT_9
    };

    int sx(0), sy(0), ix(0), iy(0);
    int text_count(0), texture_count(0);
    int font_texture_size = 2048;

    size_t short_len = swprintf(wfullpath, 256, L"Please Put All Videos in \"%s\" and Try Again.", path);
    fontBlitter.GetTextExtend(sx, sy, UI_TEXT_FONT_PYRAMID, true, wfullpath, (int)short_len);
    while (font_texture_size<sx) {
        font_texture_size *= 2;
    }
    uint8* buffer = (uint8*) blMalloc(font_texture_size*font_texture_size);
    if (NULL==buffer) {
        return false;
    }
    memset(buffer, 0, font_texture_size*font_texture_size);

    uint8* ptr = buffer;
    if (fontBlitter.DistanceMap(ptr, sx, sy, ix, iy, font_texture_size, UI_TEXT_FONT_PYRAMID, wfullpath, (int)short_len)) {
        TexCoord& tc = texcoords_[DRAW_TEXT_PUT_VIDEOS_IN_PATH];
        tc.x0 = tc.y0 = 0.0f;
        tc.x1 = (float)(sx)/(float)font_texture_size;
        tc.y1 = (float)(sy)/(float)font_texture_size;
        ++text_count;
    }
    int font_put_x = sx;
    int font_put_y = 0;
    int font_put_row = sy;

    //
    sx = font_texture_size - font_put_x;
    sy = font_texture_size - font_put_y;
    ptr = buffer + font_put_y*font_texture_size + font_put_x;
    short_len = swprintf(wfullpath, 256, L"%s", path);
    if (fontBlitter.DistanceMap(ptr, sx, sy, ix, iy, font_texture_size, UI_TEXT_FONT_PYRAMID, wfullpath, (int)short_len)) {
        TexCoord& tc = texcoords_[DRAW_TEXT_VIDEO_PATH];
        tc.x0 = (float)font_put_x/(float)font_texture_size;
        tc.y0 = (float)font_put_y/(float)font_texture_size;
        tc.x1 = (float)(font_put_x+sx)/(float)font_texture_size;
        tc.y1 = (float)(font_put_y+sy)/(float)font_texture_size;
        ++text_count;
        font_put_x += sx;
        if (font_put_row<(font_put_y+sy))
            font_put_row = (font_put_y+sy);
    }
    else {
        font_put_x = 0;
        font_put_y = font_put_row;
        ptr = buffer + font_put_y*font_texture_size;
        sx = font_texture_size;
        sy = font_texture_size - font_put_y;
        TexCoord& tc = texcoords_[DRAW_TEXT_VIDEO_PATH];
        if (fontBlitter.DistanceMap(ptr, sx, sy, ix, iy, font_texture_size, UI_TEXT_FONT_PYRAMID, wfullpath, (int)short_len)) {
            tc.x0 = (float)font_put_x/(float)font_texture_size;
            tc.y0 = (float)font_put_y/(float)font_texture_size;
            tc.x1 = (float)(font_put_x+sx)/(float)font_texture_size;
            tc.y1 = (float)(font_put_y+sy)/(float)font_texture_size;
            ++text_count;
            font_put_x = sx;
            if (font_put_row<(font_put_y+sy))
                font_put_row = (font_put_y+sy);
        }
        else {
            tc.x0 = tc.y0 = tc.x1 = tc.y1 = 0.0f; // failed
        }
    }

    for (int i=DRAW_TEXT_NO_VIDEOS; i<DRAW_TEXT_TOTALS; ++i) {
        TexCoord& tc = texcoords_[i];
        char const* text = text_out[i];
        int const len = (int) strlen(text);

        sx = font_texture_size - font_put_x;
        sy = font_texture_size - font_put_y;
        ptr = buffer + font_put_y*font_texture_size + font_put_x;

        if (fontBlitter.DistanceMap(ptr, sx, sy, ix, iy, font_texture_size, UI_TEXT_FONT_PYRAMID, text, len)) {
            tc.x0 = (float)font_put_x/(float)font_texture_size;
            tc.y0 = (float)font_put_y/(float)font_texture_size;
            tc.x1 = (float)(font_put_x+sx)/(float)font_texture_size;
            tc.y1 = (float)(font_put_y+sy)/(float)font_texture_size;
            ++text_count;
            font_put_x += sx;
            if (font_put_row<(font_put_y+sy))
                font_put_row = (font_put_y+sy);
        }
        else {
            font_put_x = 0;
            font_put_y = font_put_row;
            ptr = buffer + font_put_y*font_texture_size;
            sx = font_texture_size;
            sy = font_texture_size - font_put_y;
            if (fontBlitter.DistanceMap(ptr, sx, sy, ix, iy, font_texture_size, UI_TEXT_FONT_PYRAMID, text, len)) {
                tc.x0 = (float)font_put_x/(float)font_texture_size;
                tc.y0 = (float)font_put_y/(float)font_texture_size;
                tc.x1 = (float)(font_put_x+sx)/(float)font_texture_size;
                tc.y1 = (float)(font_put_y+sy)/(float)font_texture_size;
                ++text_count;
                font_put_x = sx;
                if (font_put_row<(font_put_y+sy))
                    font_put_row = (font_put_y+sy);
            }
            else {
                tc.x0 = tc.y0 = tc.x1 = tc.y1 = 0.0f; // failed
            }
        }
    }

    for (int i=0; i<ISO_639_TOTALS; ++i) {
        TexCoord& tc = texcoords_[DRAW_GLYPH_TOTALS+i];
        ISO_639 const cur_lan = (ISO_639) i;
        char const* text = GetNativeLanuguageUTF8(cur_lan);
        if (ISO_639_UNKNOWN!=cur_lan && lan!=cur_lan) {
            lan = cur_lan;
            fontBlitter.Create(font_name, UI_TEXT_FONT_SIZE, true, lan, UI_TEXT_FONT_DISTANCE_SPREAD_FACTOR);
        }

        int len = MultiByteToWideChar(CP_UTF8, 0, text, -1, wfilename, 256);

        // MultiByteToWideChar includes null character.
        while (len>0 && L'\0'==wfilename[len-1]) {
            --len;
        }

        if (len<=0) {
            tc.x0 = tc.x1 = tc.y0 = tc.y1 = 0.0f;
            continue;
        }

        sx = font_texture_size - font_put_x;
        sy = font_texture_size - font_put_y;
        ptr = buffer + font_put_y*font_texture_size + font_put_x;

        if (fontBlitter.DistanceMap(ptr, sx, sy, ix, iy, font_texture_size, UI_TEXT_FONT_PYRAMID, wfilename, len)) {
            tc.x0 = (float)font_put_x/(float)font_texture_size;
            tc.y0 = (float)font_put_y/(float)font_texture_size;
            tc.x1 = (float)(font_put_x+sx)/(float)font_texture_size;
            tc.y1 = (float)(font_put_y+sy)/(float)font_texture_size;
            ++text_count;
            font_put_x += sx;
            if (font_put_row<(font_put_y+sy))
                font_put_row = (font_put_y+sy);
        }
        else {
            font_put_x = 0;
            font_put_y = font_put_row;
            ptr = buffer + font_put_y*font_texture_size;
            sx = font_texture_size;
            sy = font_texture_size - font_put_y;
            if (fontBlitter.DistanceMap(ptr, sx, sy, ix, iy, font_texture_size, UI_TEXT_FONT_PYRAMID, wfilename, len)) {
                tc.x0 = (float)font_put_x/(float)font_texture_size;
                tc.y0 = (float)font_put_y/(float)font_texture_size;
                tc.x1 = (float)(font_put_x+sx)/(float)font_texture_size;
                tc.y1 = (float)(font_put_y+sy)/(float)font_texture_size;
                ++text_count;
                font_put_x = sx;
                if (font_put_row<(font_put_y+sy))
                    font_put_row = (font_put_y+sy);
            }
            else {
                break;
            }
        }
    }

    lan = ISO_639_UNKNOWN;
    fontBlitter.Create(font_name, UI_TEXT_FONT_SIZE, true, lan, UI_TEXT_FONT_DISTANCE_SPREAD_FACTOR);

    // backslash to slash
    short_len = 0;
    for (wchar_t const* wc=path; *wc!=L'\0'&&short_len<256; ++short_len) {
        wchar_t& c = wfullpath[short_len] = *wc++;
        if (c==L'\\')
            c = L'/';
    }
    wfullpath[short_len] = L'\0';
    while (short_len>0 && wfullpath[short_len-1]==L'/') {
        wfullpath[--short_len]=L'\0';
    }

    //
    // a fullpath might had been passed -- e.g. "C:/xxxx", "D:/xxxx", "E:/xxxx"...
    // or url path like "//yumei_chen_pc/TestVideoPath" -2016.11.02
    //
    // c.f.
    // #include <Shlwapi.h> (link Shlwapi.lib)
    // if (!PathFileExists(wfullpath)) {
    // }
    //
    //
    // if it's not fullpath(D:/xxxxx) nor url path(//yumei_chen_pc/...)
    //
    if (!(short_len>3 && wfullpath[1]==L':' && wfullpath[2]==L'/') &&
        !(wfullpath[0]==L'/' && wfullpath[1]==L'/')) {
        size_t full_len = GetCurrentDirectoryW(256, wfilename);
        if (0<full_len) {
            for (size_t i=0; i<full_len; ++i) {
                if (wfilename[i]==L'\\')
                    wfilename[i] = L'/';
            }

            while (full_len>0 && wfilename[full_len-1]==L'/') {
                wfilename[--full_len]=L'\0';
            }

            wchar_t const* s = wfullpath;
            wchar_t const* e = wfullpath + short_len;
            while (s<e && *s==L'.') {
                if (s[1]==L'.') {
                    if (s[2]==L'/') {
                        s += 3;
                        wchar_t* cc = wcsrchr(wfilename, L'/');
                        if (cc) {
                            *cc = L'\0';
                        }
                        else {
                            if (wfilename[1]==L':') {
                                wfilename[2] = L'\0';
                            }
                            break; //!!!
                        }
                    }
                    else {
                        break; // !!!
                    }
                }
                else {
                    ++s;
                }
            }

            if (s[0]==L'/') {
                ++s;
            }

            if (s<e) {
                // note that s points to somewhere in wfullpath.
                // so swprintf(wfullpath, 256, L"%s/%s", wfilename, s) will not work!
                //
                // need another buffer to do the job...
                wchar_t buf[256];
                short_len = swprintf(buf, 256, L"%s/%s", wfilename, s);
                memcpy(wfullpath, buf, (short_len+1)*sizeof(wchar_t));
            }
            else {
                wcscpy(wfullpath, wfilename);
            }
        }
        else {
            BL_ERR("fail to get current directory\n");
        }
    }

    // save video path
    wcscpy(video_path_, wfullpath);

    // parse config file
    swprintf(wfilename, 256, L"%s/vivecinema.xml", wfullpath);
    VRVideoConfig config(wfilename);

    struct TextBuffer {
        wchar_t* wtext;
        char*    text;
        uint32   wtext_capacity;
        uint32   wtext_write_ptr;
        uint32   text_capacity;
        uint32   text_write_ptr;
        TextBuffer():wtext(NULL),text(NULL),
            wtext_capacity(0),wtext_write_ptr(0),
            text_capacity(0),text_write_ptr(0) {
            int const init_cap = 1024*MAX_PATH;
            wtext = (wchar_t*) malloc(init_cap*sizeof(wchar_t));
            text = (char*) malloc(init_cap*sizeof(char));
            if (wtext && text) {
                wtext_capacity = text_capacity = init_cap;
                wtext[0] = text[0] = 0;
                wtext_write_ptr = text_write_ptr = 1;
            }
        }
        ~TextBuffer() {
            free(wtext);
            free(text);
        }

        char const* Get(uint32 offset) const {
            return (offset<text_write_ptr) ? (text+offset):NULL;
        }
        wchar_t const* GetW(uint32 offset) const {
            return (offset<wtext_write_ptr) ? (wtext+offset):NULL;
        }

        uint32 Put(char const* cstr, uint32& len) {
            len = 0;
            if (cstr) {
                uint32 const write_ptr_bak = text_write_ptr;
                char c = 0;
                do {
                    c = *cstr++;
                    if (text_write_ptr<text_capacity) {
                        text[text_write_ptr++] = c;
                    }
                    else {
                        uint32 const capacity2 = text_capacity*2;
                        char* text2 = (char*) malloc(capacity2*sizeof(char));
                        if (NULL==text2) {
                            text_write_ptr = write_ptr_bak;
                            return len=0;
                        }

                        memcpy(text2, text, text_write_ptr*sizeof(char));
                        free(text);
                        text = text2;
                        text_capacity = capacity2;
                        text[text_write_ptr++] = c;
                    }
                    ++len;
                } while ('\0'!=c);
                return write_ptr_bak;
            }
            return 0;
        }
        uint32 Put(wchar_t const* wstr, uint32& len) {
            len = 0;
            if (wstr) {
                uint32 const write_ptr_bak = wtext_write_ptr;
                wchar_t wc = 0;
                do {
                    wc = *wstr++;
                    if (wtext_write_ptr<wtext_capacity) {
                        wtext[wtext_write_ptr++] = wc;
                    }
                    else {
                        uint32 const capacity2 = wtext_capacity*2;
                        wchar_t* wtext2 = (wchar_t*) malloc(capacity2*sizeof(wchar_t));
                        if (NULL==wtext2) {
                            wtext_write_ptr = write_ptr_bak;
                            return len=0;
                        }

                        memcpy(wtext2, wtext, wtext_write_ptr*sizeof(wchar_t));
                        free(wtext);
                        wtext = wtext2;
                        wtext_capacity = capacity2;
                        wtext[wtext_write_ptr++] = wc;
                    }
                    ++len;
                } while (0!=wc);
                return write_ptr_bak;
            }
            return 0;
        }
    } text_buffer;

    struct VideoFile {
        uint32 wfullpath; // wide character
        uint32 url;       // utf8
        uint32 name;      // utf8
        uint32 urlLength;
        uint32 nameLength;
        uint32 sv3d;
        uint32 sa3d;
        int    timeout;
    } vfile;
    Array<VideoFile> videoFiles(1024);

    // url buffer
    char fullpath[MAX_PATH];
    char const* name = NULL;
    char const* url = NULL;

    uint32 const total_livestreams = config.GetTotalLiveStreams();
    for (uint32 i=0; i<total_livestreams; ++i) {
        uint32 timeout = 0;
        if (config.GetLiveStreamByIndex(i, name, url, vfile.sv3d, vfile.sa3d, timeout)) {
            vfile.wfullpath = 0; // no need.
            vfile.url  = text_buffer.Put(url, vfile.urlLength);
            vfile.name = text_buffer.Put(name, vfile.nameLength);
            vfile.timeout = (int) timeout;
            videoFiles.push_back(vfile);
        }
    }

    // duplicate check start
    uint32 const duplicate_video_check_begin = videoFiles.size();

    // local videos
    vfile.timeout = -1; // invalid for local videos
    uint32 const total_extend_local_videos = config.GetTotalVideos();
    for (uint32 i=0; i<total_extend_local_videos; ++i) {
        if (config.GetVideoByIndex(i, name, url, vfile.sv3d, vfile.sa3d)) {
            vfile.wfullpath = 0; // not loaded yet
            if (0==memcmp(url, "$(APP_PATH)/", 12)) {
                sprintf(fullpath, "./%s", url+12);
                url = fullpath;
            }
            vfile.url  = text_buffer.Put(url, vfile.urlLength);
            vfile.name = text_buffer.Put(name, vfile.nameLength);
            videoFiles.push_back(vfile);
        }
    }

    // duplicate check end
    uint32 const duplicate_video_check_end = videoFiles.size();

    vfile.wfullpath = 0; // TBD
    vfile.url  = vfile.urlLength = 0; // TBD
    vfile.name = vfile.nameLength = 0; // TBD
    vfile.sv3d = vfile.sa3d = 0; // always 0
    vfile.timeout = -1; // 

    int const total_ext_video_paths = config.GetTotalVideoPaths();
    for (int i=-1; i<total_ext_video_paths; ++i) { // -1 is for (default) wfullpath
        if (i>=0) {
            if (config.GetVideoPathByIndex((uint32)i, name, url)) {
                if (0==memcmp(url, "$(APP_PATH)/", 12)) {
                    sprintf(fullpath, "./%s", url+12);
                    url = fullpath;
                }
                int const chars = MultiByteToWideChar(CP_UTF8, 0, url, -1, wfullpath, 256);
                if (0>=chars && chars>250 || 0==memcmp(video_path_, wfullpath, chars*sizeof(wchar_t))) {
                    continue;
                }
            }
            else {
                continue;
            }
        }

        WIN32_FIND_DATAW fd;
        swprintf(wfilename, 256, L"%s/*.*", wfullpath);
        HANDLE hFind = FindFirstFileW(wfilename, &fd);
        if (INVALID_HANDLE_VALUE!=hFind) {
            do {
                if (0==(FILE_ATTRIBUTE_DIRECTORY&fd.dwFileAttributes) &&
                    0<GetVideoFullFilename(wfilename, 256, wfullpath, fd.cFileName)) {
                    int full_bytes = WideCharToMultiByte(CP_UTF8, 0, wfilename, -1, fullpath, MAX_PATH, NULL, NULL);

                    // duplicate check...
                    for (uint32 v=duplicate_video_check_begin; v<duplicate_video_check_end; ++v) {
                        VideoFile const& vf = videoFiles[v];
                        if (full_bytes==(int)vf.urlLength) {
                            url = text_buffer.Get(vf.url);
                            if (0==memcmp(fullpath, url, full_bytes*sizeof(char))) {
                                full_bytes = 0;
                                break;
                            }
                        }
                    }

                    if (0<full_bytes) {
                        vfile.wfullpath = text_buffer.Put(wfilename, vfile.urlLength);
                        vfile.url  = text_buffer.Put(fullpath, vfile.urlLength);
                        full_bytes = WideCharToMultiByte(CP_UTF8, 0, fd.cFileName, -1, fullpath, MAX_PATH, NULL, NULL);
                        vfile.name = text_buffer.Put(fullpath, vfile.nameLength);
                        videoFiles.push_back(vfile);
                    }
                }
            } while (0!=FindNextFileW(hFind, &fd));

            // close file
            FindClose(hFind);
        }
    }

    uint32 const total_videos = videoFiles.size();
    for (uint32 i=0; i<total_videos; ++i) {
        VideoFile const& vf = videoFiles[i];
        url = text_buffer.Get(vf.url);
        name = text_buffer.Get(vf.name);
        if (NULL==url || NULL==name || vf.url<=0)
            continue;

        // load video
        VideoTrack* video = new VideoTrack();
        if (0!=vf.wfullpath) {
            if (!video->SetFilePath(text_buffer.GetW(vf.wfullpath),
                                    url, (int)vf.urlLength,
                                    name, (int)vf.nameLength)) {
                delete video;
                continue;
            }
        }
        else if (vf.timeout<0) {
            if (MultiByteToWideChar(CP_UTF8, 0, url, -1, wfilename, 256)<=0 ||
                !video->SetFilePath(wfilename, url, (int)vf.urlLength,
                                               name, (int)vf.nameLength)) {
                delete video;
                continue;
            }
        }
        else { // live stream
            if (!video->SetLiveStream(name, url, vf.timeout)) {
                delete video;
                continue;
            }
        }

        // text glyph
        TexCoord tc;
        sx = font_texture_size - font_put_x;
        sy = font_texture_size - font_put_y;
        ptr = buffer + font_put_y*font_texture_size + font_put_x;
        int const name_len = (int)vf.nameLength - 1; // (int) strlen(name);
        assert(name_len==(int) strlen(name));
        if (fontBlitter.DistanceMap(ptr, sx, sy, ix, iy, font_texture_size, UI_TEXT_FONT_PYRAMID, name, name_len)) {
            tc.x0 = (float)font_put_x/(float)font_texture_size;
            tc.y0 = (float)font_put_y/(float)font_texture_size;
            tc.x1 = (float)(font_put_x+sx)/(float)font_texture_size;
            tc.y1 = (float)(font_put_y+sy)/(float)font_texture_size;
            ++text_count;
            font_put_x += sx;
            if (font_put_row<(font_put_y+sy))
                font_put_row = (font_put_y+sy);
        }
        else {
            if (font_put_y+sy>=font_texture_size || (font_put_row+sy)>=font_texture_size) {
                Texture2D* tex = Texture2D::New(fonts_.size());
                tex->UpdateImage((uint16) font_texture_size, (uint16) font_texture_size,
                                 FORMAT_A8, buffer, false);
                fonts_.push_back(tex);
#ifdef TEST_OUTPUT_FONT_TEXTURE
                if (texture_count>0) {
                    sprintf(fullpath, "font%d.jpg", texture_count);
                }
                else {
                    strcpy(fullpath, "font.jpg");
                }
                mlabs::balai::image::write_JPEG(fullpath, buffer, font_texture_size, font_texture_size, 1);
#endif
                memset(buffer, 0, font_texture_size*font_texture_size);
                ++texture_count;
                text_count = 0;
                font_put_y = font_put_row = 0;
            }

            font_put_x = 0;
            font_put_y = font_put_row;

            ptr = buffer + font_put_y*font_texture_size;
            sx = font_texture_size;
            sy = font_texture_size - font_put_y;

            tc.x0 = tc.y0 = tc.x1 = tc.y1 = 0.0f; // invalid texcoord
            for (int len=name_len; len>0; --len) {
                if (fontBlitter.DistanceMap(ptr, sx, sy, ix, iy, font_texture_size, UI_TEXT_FONT_PYRAMID,
                                            name, len)) {
                    tc.x0 = (float)font_put_x/(float)font_texture_size;
                    tc.y0 = (float)font_put_y/(float)font_texture_size;
                    tc.x1 = (float)(font_put_x+sx)/(float)font_texture_size;
                    tc.y1 = (float)(font_put_y+sy)/(float)font_texture_size;
                    ++text_count;
                    font_put_x = sx;
                    if (font_put_row<(font_put_y+sy))
                        font_put_row = (font_put_y+sy);
                    break;
                }
            }
            assert(font_put_x>0 || 0==name_len);
        }
        video->SetFontTexCoord(texture_count, tc);

        // tweak 
        if (0!=vf.sv3d && VIDEO_SOURCE_UNKNOWN==video->Source()) {
            VIDEO_TYPE vType = VIDEO_TYPE_2D;
            int longiSpan = 0;
            switch (vf.sv3d)
            {
            case 1: // 180
                vType = VIDEO_TYPE_SPHERICAL;
                longiSpan = 180;
                break;

            case 2: // 180 SBS
                vType = VIDEO_TYPE_SPHERICAL_3D_LEFTRIGHT;
                longiSpan = 180;
                break;

            case 3: // 180 TB
                vType = VIDEO_TYPE_SPHERICAL_3D_TOPBOTTOM;
                longiSpan = 180;
                break;

            case 4: // 360
                vType = VIDEO_TYPE_SPHERICAL;
                longiSpan = 360;
                break;

            case 5: // 360 SBS
                vType = VIDEO_TYPE_SPHERICAL_3D_LEFTRIGHT;
                longiSpan = 360;
                break;

            case 6: // 360 TB
                vType = VIDEO_TYPE_SPHERICAL_3D_TOPBOTTOM;
                longiSpan = 360;
                break;

            case 7: // SBS
                vType = VIDEO_TYPE_3D_LEFTRIGHT;
                break;

            case 8: // TB
                vType = VIDEO_TYPE_3D_TOPBOTTOM;
                break;
            }
            video->TweakVideoType(vType, longiSpan/6);
        }

        if (vf.sa3d!=0 && AUDIO_TECHNIQUE_DEFAULT==video->AudioTechnique()) {
            video->TweakAudioTechnique((1==vf.sa3d) ? AUDIO_TECHNIQUE_FUMA:AUDIO_TECHNIQUE_AMBIX);
        }

        int const video_id = playList_.size();
        if (video_id<MENU_VIDEO_THUMBNAIL_TEXTURES) {
            if (NULL==videoThumbnails_[video_id])
                videoThumbnails_[video_id] = Texture2D::New(16384+video_id, true);
            ////////////////////////////////////////////////////////////////////////////////////
            // check it out... for dynamic textures, you must set these 2 before update image 
            videoThumbnails_[video_id]->SetAddressMode(ADDRESS_CLAMP, ADDRESS_CLAMP);
            videoThumbnails_[video_id]->SetFilterMode(FILTER_BILINEAR);
            ////////////////////////////////////////////////////////////////////////////////////
        }

        playList_.push_back(video);
    }

    int const total_video_found = (int) playList_.size();
    if (total_video_found>0) {
        // thumbnail cache file
        swprintf(wfilename, 256, L"%s/playlist.info", video_path_);

        // "r+" read/update: Open a file for update (both for input and output). The file must exist.
        thumbnail_filecache_ = _wfopen(wfilename, L"r+b");
        if (NULL==thumbnail_filecache_) {
            // "w+" write/update: Create an empty file and open it for update (both for input and output).
            // If a file with the same name already exists its contents are discarded and the file is treated as a new empty file.
            thumbnail_filecache_ = _wfopen(wfilename, L"w+b");
            if (NULL==thumbnail_filecache_) {
                // is wfullpath a read-only storage!? OK... we take it.
            }
        }
        thumbnailThread_ = std::move(std::thread([this] { ThumbnailDecodeLoop_(); }));
    }

    // restore to previous locale(minimal "C")
    //setlocale(LC_CTYPE, oldlocale);

    if (text_count>0) {
        Texture2D* tex = Texture2D::New(fonts_.size());
        tex->UpdateImage((uint16) font_texture_size, (uint16) font_texture_size, FORMAT_A8, buffer, false);
        fonts_.push_back(tex);

#ifdef TEST_OUTPUT_FONT_TEXTURE
        if (texture_count>0) {
            sprintf(fullpath, "font%d.jpg", texture_count);
        }
        else {
            strcpy(fullpath, "font.jpg");
        }
        mlabs::balai::image::write_JPEG(fullpath, buffer, font_texture_size, font_texture_size, 1);
#endif
        ++texture_count;
    }

    blFree(buffer);

    // init timer/timestamp
    event_timestamp_ = subevent_timestamp_ = (float) mlabs::balai::system::GetTime();
    widget_timer_ = event_timestamp_ + menu_animation_duration_;

    // dashboard size(16:9) and xfrom
    ResetDashboardPose_();
    dashboard_width_ = 16.0f*dashboard_height_/9.0f;
    CalcDashboardThumbnailRect_();

//#define GOLDEN_BELL_AWARDS_2017_LOOPALL
#ifdef GOLDEN_BELL_AWARDS_2017_LOOPALL
    loop_mode_ = (1<total_video_found) ? 2:1;
#endif

    return total_video_found;
}

//---------------------------------------------------------------------------------------
bool VRVideoPlayer::FrameMove()
{
    // viewer transform
    Matrix3 pose;
    if (vrMgr_.GetHMDPose(pose)) {
        hmd_xform_ = pose;
    }

    // still update viewer/listener transform if lost tracking
    {
        Matrix3 rotate(0.0f, 0.0f, 0.0f);
        Vector3 const xyz = pose.Origin();
        pose.SetOrigin(0.0f, 0.0f, 0.0f);
        pose = rotate.SetEulerAngles(0.0f, 0.0f, azimuth_adjust_)*pose;
        pose.SetOrigin(xyz);
        viewer_xform_ = pose;
    }

    //
    if (quit_count_down_>0)
        --quit_count_down_;

    //
    // ponder thumbnails publishing
    int const total_videos = playList_.size();
    int const total_pages = (total_videos + MENU_VIDEO_THUMBNAILS - 1)/MENU_VIDEO_THUMBNAILS;
    int const current_page = menu_page_;
    if (0<=publishingThumbnailId_ && publishingThumbnailId_<total_videos) {
        VideoTrack* video = playList_[publishingThumbnailId_];
        if (video->IsValid()) {
            int const id0 = menu_page_*MENU_VIDEO_THUMBNAILS;
            int const id1 = ((menu_page_+total_pages-1)%total_pages)*MENU_VIDEO_THUMBNAILS;
            int const id2 = ((menu_page_+1)%total_pages)*MENU_VIDEO_THUMBNAILS;
            int i = MENU_VIDEO_THUMBNAIL_TEXTURES;
            if ((id0<=publishingThumbnailId_ && publishingThumbnailId_<(id0+MENU_VIDEO_THUMBNAILS)) ||
                (id1<=publishingThumbnailId_ && publishingThumbnailId_<(id1+MENU_VIDEO_THUMBNAILS)) ||
                (id2<=publishingThumbnailId_ && publishingThumbnailId_<(id2+MENU_VIDEO_THUMBNAILS))) {
                for (i=0; i<MENU_VIDEO_THUMBNAIL_TEXTURES; ++i) {
                    int const id = reference_tracks_[i];
                    assert(id!=publishingThumbnailId_);
                    if (-1==id ||
                        !((id0<=id && id<(id0+MENU_VIDEO_THUMBNAILS)) ||
                            (id1<=id && id<(id1+MENU_VIDEO_THUMBNAILS)) ||
                            (id2<=id && id<(id2+MENU_VIDEO_THUMBNAILS)))) {
                        if (0<=id && id<total_videos) {
                            playList_[id]->SetThumbnailTextureId(-1); // invalidate
                        }
                        videoThumbnails_[i]->UpdateImage(
                                            (uint16) VIDEO_THUMBNAIL_TEXTURE_WIDTH,
                                            (uint16) VIDEO_THUMBNAIL_TEXTURE_HEIGHT,
                                            FORMAT_RGB8,
                                            thumbnailBuffer_);
                        reference_tracks_[i] = publishingThumbnailId_;
                        video->SetThumbnailTextureId(i);
                        break;
                    }
                }
            }

            // grant the initial thumbnail publishing...
            if (i==MENU_VIDEO_THUMBNAIL_TEXTURES) {
                for (i=0; i<MENU_VIDEO_THUMBNAIL_TEXTURES; ++i) {
                    int const id = reference_tracks_[i];
                    if (-1==id) {
                        videoThumbnails_[i]->UpdateImage(
                                            (uint16) VIDEO_THUMBNAIL_TEXTURE_WIDTH,
                                            (uint16) VIDEO_THUMBNAIL_TEXTURE_HEIGHT,
                                            FORMAT_RGB8,
                                            thumbnailBuffer_);
                        reference_tracks_[i] = publishingThumbnailId_;
                        video->SetThumbnailTextureId(i);
                        break;
                    }
                }

                //if (i==MENU_VIDEO_THUMBNAIL_TEXTURES) {
                //    BL_LOG("discard thumbnail publishing(page:%d, vidoe:%d)\n", menu_page_, (int)publishingThumbnailId_);
                //}
            }
        }
        else {
            // remove this video!?
        }

        // like it or not, it must notify decoding thread
        publishingThumbnailId_ = (int) playList_.size(); // continue
        thumbnailCV_.notify_all();
    }

    // connect current focus controller
    mlabs::balai::VR::TrackedDevice const* prev_ctrl = focus_controller_;
    ConnectFocusController_();

    if (NULL==current_track_) {
        if (0x1001==on_processing_)
            on_processing_ = 0; // loading failed!!!

        if (0==(0x1000&on_processing_)) {
            return MenuUpdate_(prev_ctrl);
        }
        return true;
    }

    if (0==(0x1000&on_processing_)) {
        // update video texture, the normal case
        decoder_.UpdateFrame();
    }
    else if (0x1002==on_processing_) {
        // loading complete...

        //
        // 1) check audio setting
        //    this will callback AudioSettingCallback() to change audio device if needed
        decoder_.ConfirmAudioSetting();

        //
        // 2) prepare subtitle informat texture
        if (total_subtitle_streams_>0 && NULL!=extSubtitleInfo_) {
            assert(total_subtitle_streams_<MAX_LANGUAGE_STREAMS);
            int width(0), height(0);
            int tc[MAX_LANGUAGE_STREAMS][4];
            int const total_subtitles = (total_subtitle_streams_<MAX_LANGUAGE_STREAMS) ?
                                total_subtitle_streams_:MAX_LANGUAGE_STREAMS;

            void* pixels = decoder_.CookExtSubtitleNameTexture(width, height,
                                            tc, subStmIDs_, total_subtitles);
            if (NULL!=pixels)  {
                if (width>0 && height>0) {
                    extSubtitleInfo_->UpdateImage((uint16)width, (uint16) height, FORMAT_I8, pixels);
                    for (int i=0; i<total_subtitle_streams_; ++i) {
                        TexCoord& t = extSubtitleFilename_[i];
                        t.x0 = (float)tc[i][0]/(float)width;
                        t.x1 = (float)tc[i][1]/(float)width;
                        t.y0 = (float)tc[i][2]/(float)height;
                        t.y1 = (float)tc[i][3]/(float)height;
                    }
                }

                free(pixels);
                pixels = NULL;
            }
        }

        //
        // 3) async play
        int const duration = current_track_->GetDuration();
        int timeat = current_track_->GetTimestamp() - 5000; // recap 5 secs
        if ((timeat+15000)>duration)
            timeat = duration - 15000;
        if (timeat<15000) timeat = 0;

        on_processing_ = 0x1004; // trying to play
        on_processing_time_start_ = (float) mlabs::balai::system::GetTime();
        std::async(std::launch::async, [this,timeat] {
            decoder_.PlayAt(timeat);
            if (0x1004==on_processing_) {
                on_processing_ = 0;
            }
            else {
                BL_LOG("PlayAt(%dms) has been interrupted(0x%X)!\n", timeat, (uint32)on_processing_);
            }
        });

        // clear video texture
        videoTexture_.Resize(decoder_.VideoWidth(), decoder_.VideoHeight());

        // build OpenGL vertex array
        int longi(0), lati_south(0), lati_north(0);
        if (current_track_->GetSphericalAngles(longi, lati_south, lati_north)) {
            if (360!=longi || -90!=lati_south || 90!=lati_north) {
                customized_.Create(longi, lati_south, lati_north);
            }
        }

        // adjust azimuth angle
        if (current_track_->IsSpherical()) {
            math::Vector3 dir = hmd_xform_.YAxis();
            if (-0.8f<dir.z && dir.z<0.8f) {
                azimuth_adjust_ = atan2(dir.x, dir.y);
            }
            else {
                azimuth_adjust_ = 0.0f;
            }
        }
        ResetDashboardPose_();

        dashboard_width_ = dashboard_height_*current_track_->AspectRatio();
        //screen_width_ = screen_height_*track->AspectRatio();
        widget_width_ = 0.925f*dashboard_width_; // 0.925f*screen_width_;
        widget_left_  = -0.5f*widget_width_;

        menu_picking_slot_ = -1;
        widget_on_ = trigger_dashboard_ = trigger_dashboard_align_ = 0;
        event_timestamp_ = (float) mlabs::balai::system::GetTime();

        //BL_LOG("Video loading complete. time:%.1fms(audio:%.1fms)\n", 1000.0f*(event_timestamp_-t0), 1000.0f*(t1-t0));
        return true;
    }
    else {
        // clear video texture
        //videoTexture_.ClearBlack();

        if (0x1001==on_processing_) { // loading failed
            // next track failed, trigger another Next!?
            BL_LOG("Next track failed, one more!\n");
            on_processing_ = 0;
            NextTrack();
            return true;
        }
    }

    // current time
    float const current_time = (float) mlabs::balai::system::GetTime();

    // animation timeout
    if ((1==widget_on_ ||   // closing menu
         4==widget_on_ ||   // trigger pause/play animation
         9==widget_on_ ||   // leaving seeking bar
         12==widget_on_) && // leaving volume bar
        ((widget_timer_+animation_duration_)<current_time)) {
        widget_on_ = (widget_on_>2) ? 2:0;
    }
    else if ((16==widget_on_ || 18==widget_on_) && 
             ((widget_timer_+1.0f)<current_time)) {
        widget_on_ = 2;
    }

#ifdef BL_AVDECODER_VERBOSE
    static int last_processing = 0;
    if (on_processing_>0) {
        if (last_processing!=(int)on_processing_) {
            last_processing = (int) on_processing_;
            BL_LOG("on system processing...(0x%X)\n", last_processing);
        }
        return true;
    }
    else {
        last_processing = 0;
    }
#else
    if (on_processing_>0) {
        return true;
    }
#endif

    // controller
    bool const focus_controller_connected = (NULL!=focus_controller_);
    bool const focus_controller_persisted = focus_controller_connected && (prev_ctrl==focus_controller_);
    bool const focus_controller_tracking = focus_controller_connected && focus_controller_->IsTracked();

    // not tracking - cancel all operations need tracking
    if (!focus_controller_tracking || !focus_controller_persisted) {
        trigger_dashboard_ = trigger_dashboard_align_ = 0;
        if (3==widget_on_ ||  // touch seek bar
            5==widget_on_ ||  // seeking (pause mode)
            6==widget_on_) {  // seeking (playing mode)
            if (5==widget_on_ || 6==widget_on_) {
                bool const doplay = 6==widget_on_ && !decoder_.NearlyEnd();
                //BL_LOG("async EndVideoSeeking(%d)#1... 0x%X\n", doplay, (uint32) on_processing_);
                on_processing_ = 0x8004;
                on_processing_time_start_ = current_time;
                std::async(std::launch::async, [this,doplay] {
                    decoder_.EndVideoSeeking(doplay);
                    if (0x8004==on_processing_) {
                        on_processing_ = 0;
                    }
                    else {
                        BL_LOG("EndVideoSeeking(%d)#1 has been interrupted!(0x%X)\n", doplay, (uint32) on_processing_);
                    }
                });
            }
            widget_on_ = 4;
            widget_timer_ = current_time;
        }
        else if (11==widget_on_) { // touch volume bar
            widget_on_ = 12;
            widget_timer_ = current_time;
        }
        else if (7==widget_on_||8==widget_on_||10==widget_on_||13==widget_on_||14==widget_on_||
                 15==widget_on_||17==widget_on_) {
            widget_on_ = 2;
            widget_timer_ = 0.0f; // avoid phase in animation
        }
    }

    if (!focus_controller_connected) {
        return true;
    }

    // key input
    VR::ControllerState curr_state, prev_state;
    focus_controller_->GetControllerState(curr_state, &prev_state);

    // move dashboard, it does not need pointing test, dashboard's moving with controller
    if (curr_state.Trigger>0.0f) {
        if (trigger_dashboard_) {
            if (curr_state.OnTouchpad>1 && // pressed touch pad
                (prev_state.OnTouchpad<=1||!focus_controller_persisted)) {
                trigger_dashboard_align_ |= 1;
                subevent_timestamp_ = current_time;
            }
            if (curr_state.Grip && // pressed touch pad
                (0==prev_state.Grip ||!focus_controller_persisted)) {
                trigger_dashboard_align_ |= 2;
                subevent_timestamp_ = current_time;
            }
            RealignDashboard_(focus_controller_);
        }
    }
    else {
        // trigger released
        trigger_dashboard_ = trigger_dashboard_align_ = 0;
        if (5==widget_on_ || 6==widget_on_) {
            bool const doplay = 6==widget_on_ && !decoder_.NearlyEnd();
            //BL_LOG("async EndVideoSeeking(%d)#2...0x%X\n", doplay, (uint32) on_processing_);
            on_processing_ = 0x8004;
            on_processing_time_start_ = current_time;
            std::async(std::launch::async, [this,doplay] {
                decoder_.EndVideoSeeking(doplay);
                if (0x8004==on_processing_) {
                    //BL_LOG(" Done#2\n", doplay);
                    on_processing_ = 0;
                }
                else {
                    BL_LOG("EndVideoSeeking(%d)#2 has been interrupted(0x%X)!\n", doplay, (uint32) on_processing_);
                }
            });
            widget_on_ = 4;
            widget_timer_ = current_time;
        }
    }

    // touchpad
    if (curr_state.OnTouchpad && prev_state.OnTouchpad && curr_state.Trigger>0.0f &&
        focus_controller_persisted) {
        float dx = curr_state.Touchpad.x - prev_state.Touchpad.x;
        float dy = curr_state.Touchpad.y - prev_state.Touchpad.y;
        if (dx>0.75f)
            dx = 0.75f;
        else if (dx<-0.75f)
            dx = -0.75f;
        else if (-0.05f<dx && dx<0.05f)
            dx = 0.0f; // noise

        if (dy>0.75f)
            dy = 0.75f;
        else if (dy<-0.75f)
            dy = -0.75f;
        else if (-0.01f<dy && dy<0.01f)
            dy = 0.0f; // noise

        if (fabs(dx)>5.0f*fabs(dy)) { // x swipe
            azimuth_adjust_ += 1.0f*dx;
        }
        else if (3.0f*fabs(dx)<fabs(dy)) { // y swipe
            if (trigger_dashboard_) {
                dashboard_hit_dist_ += 7.5f*dy;
                if (dashboard_hit_dist_<dashboard_hit_distance_min)
                    dashboard_hit_dist_ = dashboard_hit_distance_min;
                else if (dashboard_hit_dist_>dashboard_hit_distance_max)
                    dashboard_hit_dist_ = dashboard_hit_distance_max;

                RealignDashboard_(focus_controller_);
            }
        }
    }

    // menu pressed : widget on/off
    // Grip button : pause/resume
    bool const menu_just_pressed = (0!=curr_state.Menu) && (0==prev_state.Menu || !focus_controller_persisted);
    bool const grip_just_pressed = (0!=curr_state.Grip) && (0==prev_state.Grip || !focus_controller_persisted);
    if (menu_just_pressed || (curr_state.Trigger<=0.0f && grip_just_pressed)) {
        widget_timer_ = current_time;
        if (widget_on_<2) { // entering widget mode
            if (grip_just_pressed && decoder_.Stop()) { // try pause
                widget_on_ = 9;
            }
            else {
                widget_on_ = 2;
            }

            if (current_track_->IsSpherical()) {
                ResetDashboardPose_();
            }
        }
        else { // leaving widget mode
            bool hold_off = false;
            if (5==widget_on_ || 6==widget_on_) {
                bool const doplay = 6==widget_on_ && !decoder_.NearlyEnd();
                //BL_LOG("async EndVideoSeeking(%d)#3...0x%X\n", doplay, (uint32) on_processing_);
                on_processing_ = 0x8004;
                on_processing_time_start_ = current_time;
                std::async(std::launch::async, [this,doplay] {
                    decoder_.EndVideoSeeking(doplay);
                    if (0x8004==on_processing_) {
                        on_processing_ = 0;
                    }
                    else {
                        BL_LOG("EndVideoSeeking(%d)#3 has been interrupted(0x%X)!\n", doplay, (uint32) on_processing_);
                    }
                });
                hold_off = !doplay;
            }

            if (grip_just_pressed || !hold_off) {
                widget_on_ = 1;
                widget_timer_ = current_time;
                decoder_.Play();
            }
        }

        return true;
    }

    // following operations need tracking...
    if (!focus_controller_tracking || (widget_on_<2 && current_track_->IsSpherical())) {
        return true;
    }

    Rectangle rect;

    // collision test is needed
    float hit_x(0.0f), hit_z(0.0f);
    float const hit_dist = DashboardPickTest_(hit_x, hit_z, focus_controller_);
    if (hit_dist<=0.0f || hit_z>widget_top_) {
        if (3==widget_on_) {  // touch seek bar
            widget_on_ = 4;
            widget_timer_ = current_time;
        }
        else if (11==widget_on_) { // touch volume bar
            widget_on_ = 12;
            widget_timer_ = current_time;
        }
        else if (7==widget_on_||8==widget_on_||10==widget_on_||13==widget_on_||14==widget_on_) {
            widget_on_ = 2;
            widget_timer_ = 0.0f; // avoid phase in animation
        }

        if (hit_dist<=0.0f) {
            if (15==widget_on_||17==widget_on_) {
                widget_on_ = 2;
                widget_timer_ = 0.0f; // avoid phase in animation
            }
            else if (5==widget_on_||6==widget_on_) {
                bool const doplay = 6==widget_on_ && !decoder_.NearlyEnd();
                //BL_LOG("async EndVideoSeeking(%d)#4...0x%X\n", doplay, (uint32) on_processing_);
                on_processing_ = 0x8004;
                on_processing_time_start_ = current_time;
                std::async(std::launch::async, [this,doplay] {
                    decoder_.EndVideoSeeking(doplay);
                    if (0x8004==on_processing_) {
                        on_processing_ = 0;
                    }
                    else {
                        BL_LOG("EndVideoSeeking(%d)#4 has been interrupted(0x%X)!\n", doplay, (uint32) on_processing_);
                    }
                });
                widget_on_ = 4;
                widget_timer_ = current_time;
            }
            return true;
        }
    }

    bool const trigger_just_pulled = (curr_state.Trigger>0.0f) && (prev_state.Trigger<=0.0f||!focus_controller_persisted);

    // trigger dashboard moving event
    if (widget_on_<2) {
        if (trigger_just_pulled) {
            // move dashboard
            dashboard_hit_dist_ = hit_dist;
            dashboard_hit_x_ = hit_x;
            dashboard_hit_z_ = hit_z;
            trigger_dashboard_ = 1;
            //trigger_dashboard_align_ = 0;
            Rumble_VRController(focus_controller_);
        }
        return true;
    }

    //
    // widget_on_>=2
    //

    // video seeking happens on entire screen
    if (5==widget_on_ || 6==widget_on_) {
        assert(curr_state.Trigger>0.0f);
        GetUIWidgetRect_(rect, DRAW_UI_SEEK_BAR);
        rect.Extend(1.0f, 1.5f);

        float proc = (hit_x-rect.x0)/(rect.x1-rect.x0);
        if (proc<0.0f) proc = 0.0f;
        else if (proc>1.0f) proc = 1.0f;
        if (fabs(widget_timer_-proc)>0.005f) {
            widget_timer_ = proc;
            on_processing_ = 0x8002;
            int const timestamp = (int) (widget_timer_*decoder_.GetDuration());
            std::async(std::launch::async, [this,timestamp] {
                decoder_.SeekVideoFrame(timestamp);
                if (0x8002==on_processing_) {
                    on_processing_ = 0;
                }
                else {
                    BL_LOG("SeekVideoFrame(%dms) has been interrupted(0x%X)!\n", timestamp, (uint32) on_processing_);
                }
            });
        }
    }

    // hit out of widget area
    if (hit_z>widget_top_) {
        if (trigger_just_pulled) {
            DRAW_GLYPH widget_type = DRAW_GLYPH_TOTALS;
            bool lan_sub_settings = false;
            int test_items = 0;
            uint8 widget_next = 0;
            if (15==widget_on_) {
                if (GetLanguageSubtitleRect_(rect, DRAW_UI_LANGUAGE, -1) &&
                    rect.In(hit_x, hit_z)) {
                    widget_next = 16;
                    widget_type = DRAW_UI_LANGUAGE;
                    test_items = total_audio_streams_;
                    lan_sub_settings = true;
                }
                else {
                    widget_on_ = 2;
                    widget_timer_ = 0.0f;
                }
            }
            else if (17==widget_on_) {
                if (GetLanguageSubtitleRect_(rect, DRAW_UI_SUBTITLE, -1) &&
                    rect.In(hit_x, hit_z)) {
                    widget_next = 18;
                    widget_type = DRAW_UI_SUBTITLE;
                    test_items = total_subtitle_streams_ + 1;
                    lan_sub_settings = true;
                }
                else {
                    widget_on_ = 2;
                    widget_timer_ = 0.0f;
                }
            }

            if (lan_sub_settings) {
                if (lan_sub_settings) {
                    for (int i=0; i<test_items; ++i) {
                        if (GetLanguageSubtitleRect_(rect, widget_type, i) &&
                            rect.In(hit_x, hit_z)) {
                            widget_on_ = widget_next;
                            menu_auto_scroll_ = (uint8) i;
                            menu_page_scroll_ = 0.5f*(rect.z0 + rect.z1);
                            subevent_timestamp_ = widget_timer_ = current_time;
                            if (widget_type==DRAW_UI_SUBTITLE) {
                                subtitle_rect_count_ = subtitle_pts_ = 0;
                                if (i<total_subtitle_streams_) {
                                    int const sid = subStmIDs_[i];
                                    current_track_->SetSubtitleStreamID(sid);
                                    std::async(std::launch::async, [this, sid] {
                                        decoder_.SetSubtitleStream(sid);
                                    });
                                }
                                else {
                                    current_track_->SetSubtitleStreamID(-1);
                                    std::async(std::launch::async, [this] {
                                        decoder_.SetSubtitleStream(-1);
                                    });
                                    widget_on_ = 2;
                                    widget_timer_ = 0.0f;
                                }
                            }
                            else {
                                if (i<total_audio_streams_) {
                                    int const aid = lanStmIDs_[i];
                                    current_track_->SetAudioStreamID(aid);
                                    std::async(std::launch::async, [this, aid] {
                                        decoder_.SetAudioStream(aid);
                                    });
                                }
                            }
                            break;
                        }
                    }
                }
            }
            else {
                // move dashboard
                dashboard_hit_dist_ = hit_dist;
                dashboard_hit_x_ = hit_x;
                dashboard_hit_z_ = hit_z;
                trigger_dashboard_ = 1;
                //trigger_dashboard_align_ = 0;
                Rumble_VRController(focus_controller_);
            }
        }
        else { // !trigger_just_pulled
            if (17==widget_on_ || 15==widget_on_) {
                DRAW_GLYPH widget = DRAW_GLYPH_TOTALS;
                int test_items = 0;
                bool is_tweaking = false;

                if (15==widget_on_ &&
                    GetLanguageSubtitleRect_(rect, DRAW_UI_LANGUAGE, -1) &&
                    rect.In(hit_x, hit_z)) {
                    test_items = total_audio_streams_;
                    widget = DRAW_UI_LANGUAGE;
                    is_tweaking = true;
                }
                else if (17==widget_on_ &&
                         GetLanguageSubtitleRect_(rect, DRAW_UI_SUBTITLE, -1) &&
                         rect.In(hit_x, hit_z)) {
                    test_items = total_subtitle_streams_ + 1;
                    widget = DRAW_UI_SUBTITLE;
                    is_tweaking = true;
                }

                if (is_tweaking) {
                    subevent_timestamp_ = current_time; // extend life
                    uint8 const old_sel = menu_auto_scroll_;
                    menu_auto_scroll_ = 0xff; // invalid
                    for (int i=0; i<test_items; ++i) {
                        if (GetLanguageSubtitleRect_(rect, widget, i) &&
                            rect.In(hit_x, hit_z)) {
                            assert(i<256);
                            if (i!=old_sel) {
                                Rumble_VRController(focus_controller_);
                            }
                            menu_auto_scroll_ = (uint8) i;
                            break;
                        }
                    }
                }
                else if ((subevent_timestamp_+animation_duration_)<current_time) {
                    widget_on_ = 2;
                    widget_timer_ = 0.0f;
                }
            }
        }

        return true;
    }

    //
    // widget_on_>=2, hit_z<=widget_top_
    //

    // trigger controls
    if (trigger_just_pulled) {
        assert(5!=widget_on_ && 6!=widget_on_);
        //if (3==widget_on_) {
            GetUIWidgetRect_(rect, DRAW_UI_SEEK_BAR);
            rect.Extend(1.0f, 1.6f);
            if (rect.In(hit_x, hit_z)) {
                if (decoder_.IsPlaying()) {
                    decoder_.Stop();
                    widget_on_ = 6;
                }
                else {
                    widget_on_ = 5;
                }

                widget_timer_ = (hit_x-rect.x0)/(rect.x1-rect.x0);
                int const attime = (int) (widget_timer_*decoder_.GetDuration());
                on_processing_ = 0x8000;
                on_processing_time_start_ = current_time;
                std::async(std::launch::async, [this,attime] {
                    decoder_.StartVideoSeeking(attime);
                    if (0x8000==on_processing_) {
                        // first seek could be slow??? (FFmpeg 3.1.5)
                        for (int i=0;
                            i<1000 && decoder_.IsSeeking() && decoder_.DecodingVideoFrame()<=0; ++i) {
                            Sleep(10);
                        }
                        on_processing_ = 0;
                    }
                    else {
                        BL_LOG("StartVideoSeeking(%dms) has been interrupted(0x%X)!\n", attime, (uint32) on_processing_);
                    }
                });
            }
        //}
        else /*if ((widget_timer_+animation_duration_)<current_time)*/ { // prevent rapid launch...
            DRAW_GLYPH const test_glyph[] = {
                DRAW_UI_REPLAY,
                DRAW_UI_PAUSE, // pause & play
                DRAW_UI_NEXT,
                DRAW_UI_MENU,
                DRAW_UI_VOLUME,
                DRAW_UI_VOLUME_BAR,
                DRAW_UI_LIGHT
            };
            for (int i=0; i<(sizeof(test_glyph)/sizeof(test_glyph[0])); ++i) {
                DRAW_GLYPH const g = test_glyph[i];
                if (GetUIWidgetRect_(rect, g)) {
                    if (rect.In(hit_x, hit_z)) {
                        switch (g) 
                        {
                        case DRAW_UI_REPLAY:
                            on_processing_ = 0x2000;
                            on_processing_time_start_ = current_time;
                            std::async(std::launch::async, [this] {
                                decoder_.Replay();
                                if (0x2000==on_processing_) {
                                    on_processing_ = 0;
                                }
                                else {
                                    BL_LOG("Replay() has been interrupted(0x%X)!\n", (uint32) on_processing_);
                                }
                            });
                            break;

                        case DRAW_UI_PAUSE:
                            if (decoder_.IsPlaying()) {
                                decoder_.Stop();
                            }
                            else {
                                decoder_.Play();
                            }
                            widget_timer_ = current_time;
                            widget_on_ = 9;
                            break;

                        case DRAW_UI_NEXT:
                            if (total_videos>1) {
                                // save timestamp
                                if (0==(0x1000&on_processing_)) {
                                    current_track_->SetTimestamp(decoder_.Timestamp());
                                }

                                int current_id = -1;
                                for (int j=0; j<total_videos; ++j) {
                                    if (playList_[j]==current_track_) {
                                        current_id = (j+1)%total_videos;
                                        break;
                                    }
                                }

                                if (0<=current_id && current_id<total_videos) {
                                    VideoTrack* video = playList_[current_id];
                                    if (Playback_(video)) {
                                        widget_timer_ = current_time;
                                        widget_on_ = 1;

                                        menu_page_ = current_id/MENU_VIDEO_THUMBNAILS;
                                        if (current_page!=menu_page_) {
                                            pageCV_.notify_one();
                                            //BL_LOG("page:%d->%d  kick off thumbnail thread\n", current_page, menu_page_);
                                        }
                                    }
                                    ///////////////////////////////
                                    // remove
                                    else {
                                        // remove this video!?
                                        video->SetFail();
                                    }
                                    ///////////////////////////////
                                }
                            }
                            break;

                        case DRAW_UI_MENU:
                            decoder_.Stop();

                            // save timestamp
                            if (0==(0x1000&on_processing_)) {
                                current_track_->SetTimestamp(decoder_.Timestamp());
                            }
                            current_track_ = NULL;
                            focus_controller_ = NULL;
                            menu_picking_slot_ = -1;

                            //dim_bkg_ = 0; keep this!
                            total_subtitle_streams_ = total_audio_streams_ = 0;
                            subtitle_rect_count_ = subtitle_pts_ = subtitle_duration_ = 0;

                            event_timestamp_ = subevent_timestamp_ = current_time;
                            widget_timer_ = current_time + menu_animation_duration_;

                            menu_page_scroll_ = 0.0f;
                            menu_auto_scroll_ = 0;
                            trigger_dashboard_ = 0;
                            trigger_dashboard_align_ = 0;
                            widget_on_ = 0;

                            //
                            // DO NOT thumbnailCV_.notify_all()! NEVER!
                            //
                            ResetDashboardPose_();
                            dashboard_width_ = 16.0f*dashboard_height_/9.0f;
                            return true;

                        case DRAW_UI_VOLUME:
                            if (masterVolume_) {
                                float const vol = masterVolume_.GetVolume(true);
                                if (vol>0.0f) {
                                    masterVolume_.SetVolume(0.0f);
                                }
                                else {
                                    masterVolume_.ResetVolume();
                                }
                            }
                            break;

                        case DRAW_UI_VOLUME_BAR:
                            if (11==widget_on_) {
                                masterVolume_.SetVolume((hit_x-rect.x0)/(rect.x1-rect.x0));
                            }
                            break;

                        case DRAW_UI_LIGHT:
                            dim_bkg_ = (dim_bkg_+1)%DIM_BACKGROUND_CONTROLS;
                            break;

                        default:
                            break;
                        }

                        break;
                    }
                }
            }
        }
    }

    if (curr_state.Trigger>0.0f && focus_controller_persisted) {
        if (11==widget_on_) {
            GetUIWidgetRect_(rect, DRAW_UI_VOLUME_BAR);
            Rectangle rect2 = rect;
            rect2.x0 = rect.x0 - 0.25f*rect.Width();
            if (rect2.In(hit_x, hit_z)) {
                float v = (hit_x-rect.x0)/(rect.x1-rect.x0);
                if (v<0.0f) v = 0.0f;
                else if (v>1.0f) v = 1.0f;
                masterVolume_.SetVolume(v);
            }
        }
    }

    if (masterVolume_) {
        GetUIWidgetRect_(rect, DRAW_UI_VOLUME);
        rect.Extend(1.02f, 1.6f);
        if (11==widget_on_) {
            Rectangle rect2;
            GetUIWidgetRect_(rect2, DRAW_UI_VOLUME_BAR);
            rect.x1 = rect2.x1 + 0.5f*(rect.x1-rect.x0);
            if (!rect.In(hit_x, hit_z)) {
                widget_on_ = 12;
                widget_timer_ = current_time;
            }
        }
        else if (rect.In(hit_x, hit_z)) {
            if (2==widget_on_) {
                masterVolume_.SyncVolume();
                widget_on_ = 11;
                widget_timer_ = current_time;
                Rumble_VRController(focus_controller_);
            }
        }
    }

    // pointing check...
    GetUIWidgetRect_(rect, DRAW_UI_SEEK_BAR);
    rect.Extend(1.02f, 1.6f);
    if (3==widget_on_) {
        rect.Extend(1.02f, 1.25f);
        if (!rect.In(hit_x, hit_z)) {
            widget_on_ = 4;
            widget_timer_ = current_time;
        }
    }
    else if (rect.In(hit_x, hit_z)) {
        if (2==widget_on_) {
            widget_on_ = 3;
            widget_timer_ = current_time;
            Rumble_VRController(focus_controller_);
        }
    }

///////////////////////////////////
// deserve a loop
    // touch replay
    if (GetUIWidgetRect_(rect, DRAW_UI_REPLAY)) {
        if (rect.In(hit_x, hit_z)) {
            if (7!=widget_on_) {
                widget_on_ = 7;
                widget_timer_ = current_time;
                Rumble_VRController(focus_controller_);
            }
        }
        else if (7==widget_on_) {
            widget_on_ = 2;
            widget_timer_ = 0.0f; // avoid phase in animation
        }
    }

    // touch pause/play
    if (GetUIWidgetRect_(rect, DRAW_UI_PAUSE)) {
        if (rect.In(hit_x, hit_z)) {
            if (8!=widget_on_) {
                widget_on_ = 8;
                widget_timer_ = current_time;
                Rumble_VRController(focus_controller_);
            }
        }
        else if (8==widget_on_) {
            widget_on_ = 2;
            widget_timer_ = 0.0f; // avoid phase in animation
        }
    }

    // touch next
    if (GetUIWidgetRect_(rect, DRAW_UI_NEXT)) {
        if (rect.In(hit_x, hit_z)) {
            if (10!=widget_on_) {
                widget_on_ = 10;
                widget_timer_ = current_time;
                Rumble_VRController(focus_controller_);
            }
        }
        else if (10==widget_on_) {
            widget_on_ = 2;
            widget_timer_ = 0.0f; // avoid phase in animation
        }
    }

    // touch light
    if (GetUIWidgetRect_(rect, DRAW_UI_LIGHT)) {
        if (rect.In(hit_x, hit_z)) {
            if (13!=widget_on_) {
                widget_on_ = 13;
                widget_timer_ = current_time;
                Rumble_VRController(focus_controller_);
            }
        }
        else if (13==widget_on_) {
            widget_on_ = 2;
            widget_timer_ = 0.0f; // avoid phase in animation
        }
    }

    // touch menu
    if (GetUIWidgetRect_(rect, DRAW_UI_MENU)) {
        if (rect.In(hit_x, hit_z)) {
            if (14!=widget_on_) {
                widget_on_ = 14;
                widget_timer_ = current_time;
                Rumble_VRController(focus_controller_);
            }
        }
        else if (14==widget_on_) {
            widget_on_ = 2;
            widget_timer_ = 0.0f; // avoid phase in animation
        }
    }

    // touch language setting
    if (GetUIWidgetRect_(rect, DRAW_UI_LANGUAGE)) {
        if (rect.In(hit_x, hit_z)) {
            if (15!=widget_on_) {
                widget_on_ = 15;
                subevent_timestamp_ = widget_timer_ = current_time;
                menu_page_scroll_ = 0.0f;
                menu_auto_scroll_ = 0xff; // invalid selection
                Rumble_VRController(focus_controller_);
            }
            subevent_timestamp_ = current_time;
        }
        else if ((15==widget_on_) &&
                 (subevent_timestamp_+animation_duration_)<current_time) {
            widget_on_ = 2;
            widget_timer_ = 0.0f;
        }
    }

    // touch subtitle. 
    if (GetUIWidgetRect_(rect, DRAW_UI_SUBTITLE)) {
        if (rect.In(hit_x, hit_z)) {
            if (17!=widget_on_) {
                widget_on_ = 17;
                subevent_timestamp_ = widget_timer_ = current_time;
                menu_page_scroll_ = 0.0f;
                menu_auto_scroll_ = 0xff; // invalid selection
                Rumble_VRController(focus_controller_);
            }
            subevent_timestamp_ = current_time;
        }
        else if (17==widget_on_ && subevent_timestamp_+animation_duration_<current_time) {
            widget_on_ = 2;
            widget_timer_ = 0.0f;
        }
    }
///////////////////////////////////////////////

    return true;
}
//---------------------------------------------------------------------------------------
bool VRVideoPlayer::Render(VR::HMD_EYE eye) const
{
    //
    // Note : this method is called twice for each loop, do NOT update status here!
    //
    if (NULL==current_track_) {
        return DrawMainMenu_(eye);
    }

    float const current_time = (float) mlabs::balai::system::GetTime();

    Renderer& renderer = Renderer::GetInstance();
    Primitives& prim = Primitives::GetInstance();

    renderer.PushState();

    // disable Z
    renderer.SetDepthWrite(false);
    renderer.SetZTestDisable();

    // draw spherical video/background
    Matrix3 view(Matrix3::Identity), pose;
    vrMgr_.GetHMDPose(pose, eye);
    pose.SetOrigin(0.0f, 0.0f, 0.0f);
    gfxBuildViewMatrixFromLTM(view, view.SetEulerAngles(0.0f, 0.0f, azimuth_adjust_)*pose);
    renderer.PushViewMatrix(view);

    // texture cache problem!?
    //renderer.InvalidTextureCache();
    //

    bool const is_360 = current_track_->IsSpherical();
    VIDEO_3D const s3D = current_track_->Stereoscopic3D();
    float crop_factor[4];
    if (current_track_->BuildSphericalCropFactor(crop_factor, eye)) {
        // 360 video - NV12
        renderer.SetEffect(pan360NV12_);
        pan360NV12_->BindConstant(pan360NV12Crop_, crop_factor);
        videoTexture_.Bind(pan360NV12_, pan360MapY_, pan360MapUV_);
        renderer.CommitChanges();

        int longi, lati_south, lati_north;
        current_track_->GetSphericalAngles(longi, lati_south, lati_north);
        if (360==longi && -90==lati_south && 90==lati_north) {
            fullsphere_.Drawcall();
        }
        else {
            customized_.Drawcall();
        }
        renderer.PopViewMatrix();
    }
    else {
        // 360 texture as background
        renderer.SetEffect(pan360_);
        pan360_->BindConstant(pan360Crop_, crop_factor);
        pan360_->BindSampler(pan360Map_, background_);
        float const dim[DIM_BACKGROUND_CONTROLS] = { 1.0f, 0.5f, 0.25f, 0.05f };
        float const val = dim[dim_bkg_%DIM_BACKGROUND_CONTROLS];
        float const diffuse[4] = { val, val, val, 1.0f };
        pan360_->BindConstant(pan360Diffuse_, diffuse);
        renderer.CommitChanges();
        fullsphere_.Drawcall();
        renderer.PopViewMatrix();
    }

    // enable Z
    renderer.SetDepthWrite(true);
    renderer.SetZTest();

    if (!is_360 || widget_on_>1) {
        renderer.SetWorldMatrix(dashboard_xform_);
        renderer.SetCullDisable();
        float const dh = 0.5f*dashboard_height_;
        float const dw = 0.5f*dashboard_width_;
        float s0 = 0.0f;
        float s1 = 1.0f;
        float t0 = 0.0f;
        float t1 = 1.0f;
        //
        //
        //
        if (VIDEO_3D_MONO!=s3D) {
            if (0==(s3D&VIDEO_3D_TOPBOTTOM)) { // SBS
                if ((VR::HMD_EYE_LEFT==eye) ^ (VIDEO_3D_LEFTRIGHT==s3D)) {
                    s0 = 0.5f;
                }
                else {
                    s1 = 0.5f;
                }
            }
            else { // TB
                //
                // cropping may be wrong, for example...
                //
                // Blu-ray Full High Definition 3D (FHD3D) 1920x2205.
                // that is 1080px2(seperate by 45 pixels height)
                // the true aspect ratio is 1.7778 but we got 1.7415
                //
                // LEFT : t0 = 0.0f, t1 = 1080/2205; 
                // RIGHT : t0 = (2205-1080)/2205.0f;
                //
                if ((VR::HMD_EYE_LEFT==eye) ^ (VIDEO_3D_TOPBOTTOM==s3D)) {
                    t0 = (1920==decoder_.VideoWidth() && 2205==decoder_.VideoHeight()) ? 0.5102f:0.5f;
                }
                else {
                    t1 = (1920==decoder_.VideoWidth() && 2205==decoder_.VideoHeight()) ? 0.4898f:0.5f;
                }
            }

            // do you feel real, if you look behind!?
            // if (pose.YAxis().Dot(dashboard_xform_.YAxis())<0.0f) {
            // }
        }

        renderer.SetEffect(videoNV12_);
        videoTexture_.Bind(videoNV12_, videoMapY_, pan360MapUV_);
        prim.BeginDraw(GFXPT_QUADLIST);
            prim.AddVertex(-dw, 0.0f, dh, s0, t0);
            prim.AddVertex(-dw, 0.0f, -dh, s0, t1);
            prim.AddVertex(dw, 0.0f, -dh, s1, t1);
            prim.AddVertex(dw, 0.0f, dh, s1, t0);
        prim.EndDraw();

        // on processing...
        if (0!=on_processing_ && (on_processing_time_start_+0.25f)<=current_time) {
            renderer.SetDepthWrite(false);
            renderer.SetZTestDisable();
            renderer.SetBlendMode(GFXBLEND_SRCALPHA, GFXBLEND_INVSRCALPHA);
            prim.BeginDraw(NULL, GFXPT_QUADLIST);
                Color const clr = { 0, 0, 0, 160 };
                prim.SetColor(clr);
                prim.AddVertex(-dw, 0.0f, dh);
                prim.AddVertex(-dw, 0.0f, -dh);
                prim.AddVertex(dw, 0.0f, -dh);
                prim.AddVertex(dw, 0.0f, dh);
            prim.EndDraw();

            //
            // TO-DO : animation followed...
            //
            // animation_time = current_time - (on_processing_time_start_+0.25f);
            //
            prim.BeginDraw(uiGlyph_, GFXPT_QUADLIST);
            TexCoord const& tc = texcoords_[DRAW_UI_BUSY];
            float x0, x1, x2, x3, z0, z1, z2, z3;
            float const size = 0.2f*(dw+dh);
            BusySquare(x0, z0, x1, z1, x2, z2, x3, z3,
                       size, current_time-on_processing_time_start_-0.25f);
            prim.AddVertex(x0, 0.0f, z0, tc.x0, tc.y0);
            prim.AddVertex(x1, 0.0f, z1, tc.x0, tc.y1);
            prim.AddVertex(x2, 0.0f, z2, tc.x1, tc.y1);
            prim.AddVertex(x3, 0.0f, z3, tc.x1, tc.y0);
            prim.EndDraw();

            renderer.SetBlendMode(GFXBLEND_ONE, GFXBLEND_ZERO);
            renderer.SetDepthWrite(true);
            renderer.SetZTest();
        }

        // subtitle (plane movie)
        int const timestamp = decoder_.Timestamp();
        int const subtitle_time = timestamp - subtitle_pts_;
        if (0<subtitle_time && subtitle_time<subtitle_duration_ && subtitle_ && subtitle_rect_count_>0) {
            Rectangle screen;
            screen.x0 = -dw; screen.x1 = dw;
            screen.z0 = dh; screen.z1 = -dh;
            DrawSubtitle_(dashboard_xform_, timestamp, screen, true, false);
        }

        // show widgets here...
        if (widget_on_)
            DrawWidgets_(eye);

        renderer.SetZTestDisable();
        prim.BeginDraw(NULL, GFXPT_LINELIST);
        if (trigger_dashboard_) {
            float const border = 0.2f;
            float const x0 = -dw - border;
            float const z0 = -dh - border;
            float const x1 = dw + border;
            float const z1 = dh + border;
            prim.SetColor(Color::Cyan);
            prim.AddVertex(x0, 0.0f, z1);
            prim.AddVertex(x0, 0.0f, z0);

            prim.AddVertex(x0, 0.0f, z0);
            prim.AddVertex(x1, 0.0f, z0);

            prim.AddVertex(x1, 0.0f, z0);
            prim.AddVertex(x1, 0.0f, z1);

            prim.AddVertex(x1, 0.0f, z1);
            prim.AddVertex(x0, 0.0f, z1);
        }

        // draw targeting line, collect max 2 cursor dots.
        Vector3 hit[2];
        Color colors[2];
        float dists[2];
        int hit_points = 0;
        for (int i=0; (0==on_processing_)&&i<2; ++i) {
            VR::TrackedDevice const* device = vrMgr_.GetTrackedDevice(i);
            if (device && device->IsTracked()) {
                float hit_x, hit_z;
                float const dist = DashboardPickTest_(hit_x, hit_z, device);
                if (0.0f<dist) {
                    bool const widgets_pointing = (hit_z<widget_top_ && widget_on_>=2) || 15==widget_on_ || 17==widget_on_;
                    if ((trigger_dashboard_ || widgets_pointing) && device==focus_controller_) {
                        prim.SetColor(viveColor_);
                        Matrix3 const xform = dashboard_xform_.GetInverse() * device->GetPose();
                        prim.AddVertex(xform.Origin());
                        prim.AddVertex(xform.PointTransform(Vector3(0.0f, dist, 0.0f)));
                        colors[hit_points] = viveColor_;
                    }
                    else {
                        colors[hit_points] = Color::Gray;
                        colors[hit_points].a = 128;
                    }

                    dists[hit_points] = dist;
                    Vector3& h = hit[hit_points];
                    h.x = hit_x;
                    h.y = -0.001f;
                    h.z = hit_z;

                    if (device!=focus_controller_ || (5!=widget_on_ && 6!=widget_on_)) {
                        ++hit_points;
                    }
                }
                else if (device->GetAttention()) {
                    prim.SetColor(viveColor_);
                    Matrix3 const xform = dashboard_xform_.GetInverse() * device->GetPose();
                    prim.AddVertex(xform.Origin());
                    prim.AddVertex(xform.PointTransform(Vector3(0.0f, 100, 0.0f)));
                }
            }
        }
        prim.EndDraw();

        if (hit_points>0) {
            renderer.PushState();
            renderer.SetBlendMode(GFXBLEND_SRCALPHA, GFXBLEND_INVSRCALPHA);
            renderer.SetDepthWrite(false);
            renderer.SetCullDisable();
            renderer.SetZTestDisable();
            prim.BeginDraw(uiGlyph_, GFXPT_QUADLIST);
            TexCoord const& tc = texcoords_[DRAW_UI_CIRCLE];
            for (int i=0; i<hit_points; ++i) {
                Vector3 const& h = hit[i];
                prim.SetColor(colors[i]);
                float const sh = 0.5f * 0.3f*sqrt(dists[i]/dashboard_distance_); // roughly compensate
                float const sw = sh*(tc.x1-tc.x0)/(tc.y1-tc.y0);
                float x0 = h.x - sw;
                float x1 = h.x + sw;
                float z0 = h.z + sh;
                float z1 = h.z - sh;
                prim.AddVertex(x0, h.y, z0, tc.x0, tc.y0);
                prim.AddVertex(x0, h.y, z1, tc.x0, tc.y1);
                prim.AddVertex(x1, h.y, z1, tc.x1, tc.y1);
                prim.AddVertex(x1, h.y, z0, tc.x1, tc.y0);
            }
            prim.EndDraw();

            renderer.PopState();
        }

        renderer.SetZTest();
    }
    else {
        //
        // is_360 && widget_on_<=1
        //

        //
        // on processing...
        //
        if (0!=on_processing_ && (on_processing_time_start_+0.25f)<=current_time) {/*
            renderer.SetDepthWrite(false);
            renderer.SetZTestDisable();
            renderer.SetBlendMode(GFXBLEND_SRCALPHA, GFXBLEND_INVSRCALPHA);
            prim.BeginDraw(NULL, GFXPT_QUADLIST);
                Color const clr = { 0, 0, 0, 160 };
                prim.SetColor(clr);
                prim.AddVertex(-dw, 0.0f, dh);
                prim.AddVertex(-dw, 0.0f, -dh);
                prim.AddVertex(dw, 0.0f, -dh);
                prim.AddVertex(dw, 0.0f, dh);
            prim.EndDraw();

            //
            // TO-DO : animation followed...
            //
            // animation_time = current_time - (on_processing_time_start_+0.25f);
            //
            prim.BeginDraw(uiGlyph_, GFXPT_QUADLIST);
            TexCoord const& tc = texcoords_[DRAW_UI_BUSY];
            float x0, x1, x2, x3, z0, z1, z2, z3;
            float const size = 0.2f*(dw+dh);
            BusySquare(x0, z0, x1, z1, x2, z2, x3, z3,
                       size, current_time-on_processing_time_start_-0.25f);
            prim.AddVertex(x0, 0.0f, z0, tc.x0, tc.y0);
            prim.AddVertex(x1, 0.0f, z1, tc.x0, tc.y1);
            prim.AddVertex(x2, 0.0f, z2, tc.x1, tc.y1);
            prim.AddVertex(x3, 0.0f, z3, tc.x1, tc.y0);
            prim.EndDraw();

            renderer.SetBlendMode(GFXBLEND_ONE, GFXBLEND_ZERO);
            renderer.SetDepthWrite(true);
            renderer.SetZTest();*/
        }

        // subtitle 360
        int const timestamp = decoder_.Timestamp();
        int const subtitle_time = timestamp - subtitle_pts_;
        if (subtitle_ && subtitle_rect_count_>0 && subtitle_time<subtitle_duration_) {
            if (subtitle_time>0) {
                //
                // determine display pannel size...
                // with 3 meters width display, 5 meters away,
                // it covers about 60 degree(atan(3/5)) horizontal FOV.
                float const dw = 3.0f; // 3 meters width display
                float const dh = dw*decoder_.VideoHeight()/decoder_.VideoWidth();
                float const adjh = -0.2f*dh; // sightly height adjustment
                Rectangle screen;
                screen.x0 = -dw;
                screen.x1 = dw;
                screen.z0 = dh + adjh;
                screen.z1 = -dh + adjh;
                DrawSubtitle_(text_xform_, timestamp, screen, false, true);
            }

            if (subtitle_time<250) {
                Matrix3 ltm;
                AlignSubtitle360Transform_(ltm);
                if (0<subtitle_time) {
                    //
                    // follow line of sight?
                    //
                }
                else {
                    text_xform_ = ltm;
                }
            }
        }
        else {
            if (widget_on_) { // widget_on_ = 1, fadeout anumation
                DrawWidgets_(eye);
            }
            else {
            }
        }
    }

    // draw 2 controller models
    for (int i=0; i<2; ++i) {
        VR::TrackedDevice const* device = vrMgr_.GetTrackedDevice(i);
        if (device)
            device->DrawSelf();
    }

    // video (short) file name
    if (IsPlaying() || on_processing_>=1002) {
        if ((event_timestamp_+show_new_video_filename_period)>current_time) {
            float const fade_out_time = 0.8f*show_new_video_filename_period;
            float const fade_out_start = event_timestamp_ + show_new_video_filename_period - fade_out_time;
            float alpha = 1.0f;
            if (fade_out_start<current_time) {
                alpha = 1.0f - (current_time - fade_out_start)/fade_out_time;
                alpha *= alpha; // non linear
            }
            if (0.0f<alpha && alpha<=1.0f) {
                int tex_id = 0;
                TexCoord const& tc = current_track_->GetFontTexCoord(tex_id);
                if (0<=tex_id && tex_id<(int)fonts_.size()) {
                    Color clr = { 255, 255, 255, uint8(alpha*255) };
                    DrawText_(text_xform_, fonts_[tex_id], best_text_size_view_in_1m, clr, tc);
                }
            }
        }
    }

    renderer.PopState();

    return (0!=quit_count_down_);
}

//---------------------------------------------------------------------------------------
float VRVideoPlayer::DrawVideoPathOnScreen(float x, float y, float h, float screen_aspect_ratio,
                                           mlabs::balai::graphics::Color const& color, uint32 /*align*/) const {
    TexCoord const& tc = texcoords_[DRAW_TEXT_VIDEO_PATH];
    if (h>0.0f && tc.x0<tc.x1 && tc.y0<tc.y1) {
        float const w = h*tc.AspectRatio()/screen_aspect_ratio;
        mlabs::balai::graphics::Renderer& renderer = Renderer::GetInstance();
        mlabs::balai::graphics::Primitives& prim = mlabs::balai::graphics::Primitives::GetInstance();

        renderer.PushState();
        renderer.SetCullDisable();
        renderer.SetDepthWrite(false);
        renderer.SetZTestDisable();
        renderer.SetBlendMode(mlabs::balai::graphics::GFXBLEND_SRCALPHA, mlabs::balai::graphics::GFXBLEND_INVSRCALPHA);
        renderer.SetEffect(fontFx_);
        fontFx_->SetSampler(shader::semantics::diffuseMap, fonts_[0]);
        prim.BeginDraw(mlabs::balai::graphics::GFXPT_SCREEN_QUADLIST);
            prim.SetColor(color);
            prim.AddVertex2D(x, y, tc.x0, tc.y0);
            prim.AddVertex2D(x, y+h, tc.x0, tc.y1);
            prim.AddVertex2D(x+w, y+h, tc.x1, tc.y1);
            prim.AddVertex2D(x+w, y, tc.x1, tc.y0);
        prim.EndDraw();
        renderer.PopState();

        return w;
    }
    return 0.0f;
}

} // namespace htc