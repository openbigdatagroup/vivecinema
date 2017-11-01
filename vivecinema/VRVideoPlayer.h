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
 * @file    VRVideoPlayer.h
 * @author  andre chen
 * @history 2015/12/09 created
 *
 */
#ifndef VR_VIDEOPLAYER_H
#define VR_VIDEOPLAYER_H

#include "AVDecoder.h"

#include "BLVR.h"
#include "BLGLShader.h"
#include "BLOpenGL.h"
#include "BLColor.h"
#include "BLStringPool.h"

#include "win32_helper.h"

#include <future> // std::async

// Viewer Discretion Is Advised
#ifndef HTC_VIVEPORT_RELEASE
#define VIEWER_DISCRETION_IS_ADVICED
#endif

using namespace mlabs::balai;
using namespace mlabs::balai::video;

namespace htc {

struct TexCoord {
    float x0, y0; // top-left corner
    float x1, y1; // bottom-right corner
    TexCoord():x0(0.0f),y0(0.0f),x1(0.0f),y1(0.0f) {}
    float AspectRatio() const { return (x1-x0)/(y1-y0); }
};

// pick test
float PickTest(float& hit_x, float& hit_z,
               mlabs::balai::math::Matrix3 const& xform,
               mlabs::balai::VR::TrackedDevice const*);

inline void BusySquare(float& x0, float& z0,
                       float& x1, float& z1,
                       float& x2, float& z2,
                       float& x3, float& z3,
                       float size, float t) {

    // adjust speed and cyclic in range [0, 1].
    t = fmod(0.66f*t, 1.0f);

    // smoothstep
    t = t*t*(3 - 2*t);
  //t = t*t*t*(t*(t*6.0f - 15.0f) + 10.0f); // Ken Perln style

    // range[0, 2pi] for a round(negate for correct winding)
    t *= -mlabs::balai::math::constants::float_two_pi;
    size *= 0.5f;
    x3 = size*cos(t);
    z3 = size*sin(t);
    x0 = -x3-z3; z0 = x3-z3;
    // by symmetric...
    x1 = -z0; z1 = x0;
    x2 = -x0; z2 = x1;
    x3 =  z0; z3 = x2;
}

// rumble controller
inline void Rumble_VRController(mlabs::balai::VR::TrackedDevice const* ctrl) {
    if (ctrl) {
        ctrl->SetCommand(mlabs::balai::VR::TRACKED_DEVICE_COMMAND_RUMBLE, 0, 1000);
    }
}

enum VIDEO_SOURCE {
    VIDEO_SOURCE_UNKNOWN = 0,

    // https://github.com/google/spatial-media/blob/master/docs/spherical-video-rfc.md
    VIDEO_SOURCE_GOOGLE_JUMP = 1,
    VIDEO_SOURCE_GOOGLE_JUMP_RFC2 = (1<<1)|VIDEO_SOURCE_GOOGLE_JUMP,

    VIDEO_SOURCE_USER_DEFINED = (1<<3),

    // more normal sources to come...

    // nasty video types
    VIDEO_SOURCE_VIEWER_DISCRETION_IS_ADVICED = (1<<16),

    // www.virtualrealporn.com
    VIDEO_SOURCE_VIRTUALREAL_PORN = VIDEO_SOURCE_VIEWER_DISCRETION_IS_ADVICED | 1,

    // more nasty sources to come...
};

enum VIDEO_TYPE {
    VIDEO_TYPE_2D = mlabs::balai::video::VIDEO_3D_MONO, // 0

    VIDEO_TYPE_3D_TOPBOTTOM = VIDEO_3D_TOPBOTTOM, // 1
    VIDEO_TYPE_3D_LEFTRIGHT = VIDEO_3D_LEFTRIGHT, // 2
    VIDEO_TYPE_3D_BOTTOMTOP = VIDEO_3D_BOTTOMTOP, // 3
    VIDEO_TYPE_3D_RIGHTLEFT = VIDEO_3D_RIGHTLEFT, // 4

    VIDEO_TYPE_3D_MASK = 7,

    // spherical video
    VIDEO_TYPE_SPHERICAL = 1<<4,

    // Google's Jump VR
    VIDEO_TYPE_SPHERICAL_3D_TOPBOTTOM = VIDEO_TYPE_SPHERICAL | VIDEO_TYPE_3D_TOPBOTTOM,

    // this may be an ill format for entire 360 scene, but, this is also a
    // strongly desired format accoding to viveport users feedback.
    VIDEO_TYPE_SPHERICAL_3D_LEFTRIGHT = VIDEO_TYPE_SPHERICAL | VIDEO_TYPE_3D_LEFTRIGHT,
};

// spatial audio 3D
struct SA3D {
    AUDIO_TECHNIQUE Technique;
    uint16          TotalStreams;
    uint16          TotalChannels;
    uint8           StreamIds[4];   // 0xff if invalid
    uint8           ChannelCnt[4];  // 0 for unknown(respect to StreamIds[] if valid)
    uint8           Indices[16];    // indices

    SA3D() { Reset(); }
    void Reset() {
        Technique = AUDIO_TECHNIQUE_DEFAULT; // invalid
        TotalStreams = TotalChannels = 0;
        memset(StreamIds, 0xff, sizeof(StreamIds));
        memset(ChannelCnt, 0, sizeof(ChannelCnt));
        memset(Indices, 0xff, sizeof(Indices));
    }
};

class VideoTrack
{
    SA3D         sa3d_;
    TexCoord     texcoord_; // font texture coordniate
    VIDEO_TYPE   type_;
    VIDEO_TYPE   type_intrinsic_;
    VIDEO_SOURCE source_;
    char*      name_;     // utf8 (short file) name
    char*      fullpath_; // utf8 (to open video, avformat_open_input() expect utf-8 encoding)
    int        fullpath_bytes_;  // filename bytes, include ending null-character.
    int        thumbnail_texture_id_;
    int        font_texture_id_;
    int        width_;
    int        height_;
    int        spherical_longitude_;
    int        spherical_latitude_south_;
    int        spherical_latitude_north_;
    int        thumbnail_cache_;
    int        timeout_;  // if lag, the timeout to restart in milliseconds
    int        duration_; // read from formatCtx->duration, in milliseconds.
    mutable int duration_end_; // duration set when play end
    mutable int timestamp_;
    mutable uint8 subtitle_stream_id_; // 0:not specified 0xff:disabled
    mutable uint8 audio_stream_id_;    // 0:not specified 0xff:disabled
    mutable uint8 non_diegetic_audio_stream_id_;    // 0:not specified 0xff:disabled
    uint8      status_;  // 0:N/A, 1:ready, 2:error
    uint8      full3D_;  // full frame of half frame(scaled) stereoscope 3D
    bool       liveStream_;
    bool       spatialAudioWithNonDiegetic_;

    BL_NO_COPY_ALLOW(VideoTrack);

public:
    VideoTrack();
    ~VideoTrack();

    // font texture coordinate
    TexCoord const& GetFontTexCoord(int& font_id) const {
        font_id = font_texture_id_;
        return texcoord_;
    }

    // video type & tweak
    VIDEO_TYPE Type() const { return type_; }
    VIDEO_TYPE Type_Intrinsic() const { return type_intrinsic_; }
    VIDEO_SOURCE Source() const { return source_; }
    bool IsLiveStream() const { return liveStream_; }

    // time is the period to restart playback, in milliseconds.
    bool IsTimeout(int time) const { return time>timeout_; }
    void SetTimeout(int timeout) { timeout_ = timeout>250 ? timeout:250; }

    bool ViewerDiscretionIsAdviced() const {
#ifdef VIEWER_DISCRETION_IS_ADVICED
        return 0!=(VIDEO_SOURCE_VIEWER_DISCRETION_IS_ADVICED&source_);
#else
        return false;
#endif
    }

    bool IsSpherical() const { return 0!=(VIDEO_TYPE_SPHERICAL&type_); }
    VIDEO_3D Stereoscopic3D() const {
        return (VIDEO_3D) (type_ & VIDEO_TYPE_3D_MASK);
    }

    AUDIO_TECHNIQUE AudioTechnique() const { return sa3d_.Technique; }
    bool TweakAudioTechnique(AUDIO_TECHNIQUE tech) {
        sa3d_.Reset();
        sa3d_.Technique = tech;
        if (AUDIO_TECHNIQUE_DEFAULT!=sa3d_.Technique) {
            sa3d_.TotalStreams = 1;
        }
        return true;
    }
    void GetVideoParams(VideoOpenOption& param) const {
        param.Reset();

        // 0xfe is the special code for turning off!
        if (0xfe!=audio_stream_id_)
            param.AudioStreamId = audio_stream_id_;
        if (0xfe!=subtitle_stream_id_)
            param.SubtitleStreamId = subtitle_stream_id_;

        //
        // in case AudioSetting could be wrong. (e.g. technique and channels not match)
        // we must call AVDecoder::ConfirmAudioSetting() to fix it. see 
        // VRVideoPlayer::AudioSettingCallback() for more details.
        //
        if (AUDIO_TECHNIQUE_DEFAULT!=sa3d_.Technique) {
            param.AudioSetting.Technique = sa3d_.Technique;
            param.AudioSetting.Format = AUDIO_FORMAT_F32;
            param.AudioSetting.SampleRate = 48000;
            for (int i=0; i<sizeof(param.AudioSetting.StreamIndices)&&i<sa3d_.TotalStreams; ++i) {
                param.AudioSetting.StreamIndices[i] = sa3d_.StreamIds[i];
                param.AudioSetting.StreamChannels[i] = sa3d_.ChannelCnt[i];
            }
            param.AudioSetting.TotalStreams = sa3d_.TotalStreams;
            param.AudioSetting.TotalChannels = sa3d_.TotalChannels;
        }
        else {
            if (0!=(VIDEO_TYPE_SPHERICAL&type_)) { // definitely need to check this...
                param.AudioSetting.Technique = AUDIO_TECHNIQUE_AMBIX;
                param.AudioSetting.Format = AUDIO_FORMAT_F32;
                param.AudioSetting.SampleRate = 48000;
            }
        }
    }

    bool TweakVideo3D(VIDEO_3D s3D) {
        //timestamp_ = 0;
        if (type_ & VIDEO_TYPE_SPHERICAL) {
            if (VIDEO_3D_MONO==s3D ||
                VIDEO_3D_TOPBOTTOM==s3D ||
                VIDEO_3D_LEFTRIGHT==s3D) {
                type_ = (VIDEO_TYPE) (VIDEO_TYPE_SPHERICAL | s3D);
            }
            else {
                return false;
            }
        }
        else {
            type_ = (VIDEO_TYPE) s3D;
        }
        return true;
    }
    void TweakVideoSpherical() {
        //timestamp_ = 0;
        if (type_ & VIDEO_TYPE_SPHERICAL) {
            type_ = (VIDEO_TYPE) (type_ & VIDEO_TYPE_3D_MASK);
            spherical_longitude_ = 0;
            spherical_latitude_south_ = 0;
            spherical_latitude_north_ = 0;
        }
        else {
            if (type_==VIDEO_TYPE_2D) {
                type_ = VIDEO_TYPE_SPHERICAL;
            }
            else if (type_ & VIDEO_TYPE_3D_TOPBOTTOM) {
                type_ = VIDEO_TYPE_SPHERICAL_3D_TOPBOTTOM;
            }
            else {
                type_ = VIDEO_TYPE_SPHERICAL_3D_LEFTRIGHT;
            }
            spherical_longitude_ = 360;
            spherical_latitude_south_ = -90;
            spherical_latitude_north_ = 90;
        }
    }
    bool TweakVideoSphericalLongitudeSpan(int longitude_span=360) { // menu widget used only
        if (type_ & VIDEO_TYPE_SPHERICAL) {
            spherical_longitude_ = longitude_span;
            return true;
        }
        return false;
    }
    bool TweakVideoType(VIDEO_TYPE type, int param) {
        switch (type)
        {
        case VIDEO_TYPE_2D:
        case VIDEO_TYPE_3D_LEFTRIGHT:
        case VIDEO_TYPE_3D_TOPBOTTOM:
            spherical_longitude_ = 0;
            spherical_latitude_south_ = 0;
            spherical_latitude_north_ = 0;
            type_ = type;
            break;

        case VIDEO_TYPE_SPHERICAL:
        case VIDEO_TYPE_SPHERICAL_3D_TOPBOTTOM:
        case VIDEO_TYPE_SPHERICAL_3D_LEFTRIGHT:
            spherical_longitude_ = (30!=param) ? 360:180;
            spherical_latitude_south_ = -90;
            spherical_latitude_north_ = 90;
            type_ = type;
            break;

        default:
            return false;
        }
        return true;
    }/*
    bool SetSphericalAngles(VIDEO_TYPE type, int longi_span, int lati_south, int lati_north) {
        if (0!=(VIDEO_TYPE_SPHERICAL&type) && longi_span>15 &&
            -90<=lati_south && lati_south<lati_north && lati_north<=90) {
            type_ = type;
            spherical_longitude_ = longi_span;
            spherical_latitude_south_ = lati_south;
            spherical_latitude_north_ = lati_north;
            source_ = VIDEO_SOURCE_USER_DEFINED;
            return true;
        }
        return false;
    }*/
    bool GetSphericalAngles(int& longi, int& lati_south, int& lati_north) const {
        if (0!=(VIDEO_TYPE_SPHERICAL&type_)) {
            longi = spherical_longitude_;
            lati_south = spherical_latitude_south_;
            lati_north = spherical_latitude_north_;
            return true;
        }
        return false;
    }
    bool BuildSphericalCropFactor(float coeff[4], mlabs::balai::VR::HMD_EYE eye) const {
        // full equirectangular
        coeff[0] = 0.5f; // horizontal center texcoord
        coeff[1] = 0.15915494309189533576888376337251f; // horizontal texcoord scale:1/2pi
        coeff[2] = 0.0f; // latitude top texcoord
        coeff[3] = 0.31830988618379067153776752674503f; // vertical texcoord scale:1/pi
        if (0!=(VIDEO_TYPE_SPHERICAL&type_)) {
            switch (type_&VIDEO_TYPE_3D_MASK)
            {
            case VIDEO_3D_LEFTRIGHT:
                coeff[0] = (mlabs::balai::VR::HMD_EYE_RIGHT==eye) ? 0.75f:0.25f;
                coeff[1] = 0.07957747154594766788444188168626f; // 0.5/2pi
                break;

            case VIDEO_3D_TOPBOTTOM:
                coeff[2] = (mlabs::balai::VR::HMD_EYE_RIGHT==eye) ? 0.5f:0.0f;
                coeff[3] = coeff[1]; // 0.5/pi
                break;

            default:
                break;
            }

            // texcoord offset/scaling if not cover 360x180
            if (spherical_longitude_<360) {
                coeff[1] *= 360.0f/float(spherical_longitude_);
            }

            assert(spherical_latitude_south_<spherical_latitude_north_);
            if (-90<spherical_latitude_south_ || spherical_latitude_north_<90) {
                coeff[3] *= 180.0f/float(spherical_latitude_north_-spherical_latitude_south_);
                if (spherical_latitude_north_<90) {
                    coeff[2] = 0.0174532925f*(((float)spherical_latitude_north_-90.0f))*coeff[3];
                }
            }

            return true;
        }
        return false;
    }

    // full path name. to open file
    char const* GetFullPath() const { return fullpath_; }
    int GetFullPathBytes() const { return fullpath_bytes_; }

    // (short file) name (utf-8)
    char const* GetName() const { return name_; }

    // Don't call me! exclusive for VRVideoPlayer::ThumbnailDecodeLoop_()
    int& ThumbnailCache() { return thumbnail_cache_; }
    uint8& Subtitle() { return subtitle_stream_id_; }
    uint8& Audio() { return audio_stream_id_; }

    void SetVeryEnd(int endTime) const {
        timestamp_ = 0;
        if (endTime>0) {
            if ((endTime+5000)<duration_) {
                duration_end_ = endTime;

                //
                // mark status_ to indicate this video has a shorter duration.
                //
            }
        }
    }
    int GetDuration() const { return (0<duration_end_&&duration_end_<duration_) ? duration_end_:duration_; }
    int SetDuration(int t) { duration_ = t; return duration_end_; }
    int GetTimestamp() const { return timestamp_; }
    void SetTimestamp(int t) const { if (0<=t&&t<duration_) timestamp_ = t; }
    bool SetSubtitleStreamID(int sid) const {
        if (-1<=sid && sid<255) {
            subtitle_stream_id_ = (sid>=0) ? ((uint8)sid):0xff;
            return true;
        }
        return false;
    }
    bool SetAudioStreamID(int aid) const {
        if (-1<=aid && aid<255) {
            audio_stream_id_ = (aid>=0) ? ((uint8)aid):0xff;
            return true;
        }
        return false;
    }

    int ThumbnailTextureId() const { return thumbnail_texture_id_; }
    void SetThumbnailTextureId(int id) { thumbnail_texture_id_ = id; }

    bool SetFontTexCoord(int texId, TexCoord const& tc) {
        if (tc.x0<tc.x1 && tc.y0<tc.y1 && 0<=texId) {
            texcoord_ = tc;
            font_texture_id_ = texId;
            return true;
        }
        texcoord_ = TexCoord();
        font_texture_id_ = -1;
        return false;
    }

    int Width() const { return width_; }
    int Height() const { return height_; };
    float AspectRatio() const { // aspect ration in plane mode
        assert(height_>0);
        if (height_>0) {
            if (type_&VIDEO_TYPE_SPHERICAL) {
                // for equalrectangular projection and assuming pixel aspect ration = 1:1
                return (float)spherical_longitude_/(float)(spherical_latitude_north_-spherical_latitude_south_);
            }
            else {
                float aspect_ratio = float(width_)/float(height_);
                if (1!=full3D_) {
                    //
                    // some cinema likely to have 2.4:1, e.g. 1280x532, 1920x792
                    //
                    VIDEO_3D const s3D = Stereoscopic3D();
                    if (VIDEO_3D_MONO!=s3D) {
                        if (0!=(s3D&VIDEO_3D_TOPBOTTOM)) {
                            // Blu-ray Full High Definition 3D (FHD3D) 1920x2205.
                            // that is 1080px2(seperate by 45 pixels height)
                            if (1920==width_ && 2205==height_) {
                                aspect_ratio = 1920.0f/1080.0f;
                            }
                            else if (2==full3D_ || aspect_ratio<1.0f) {
                                aspect_ratio *= 2.0f; // guess
                            }
                        }
                        else {
                            if (2==full3D_ || aspect_ratio>2.5f)
                                aspect_ratio *= 0.5f; // guess
                        }
                    }
                }
                return aspect_ratio;
            }
        }
        return 1.6667f; // a guess... return 0.0 is not a good idea!
    }
    void SetSize(int w, int h) { // pixel size
        assert(w && h);
        width_ = w; height_ = h;

        // Blu-ray Full High Definition 3D (FHD3D) 1920x2205.
        // that is 1080px2(seperate by 45 pixels height)
        if (1920==w && 2205==h) {
            full3D_ = 2;
        }
    }

    void SetFail() { status_ = 2; };
    bool IsValid() const { return (1==status_); }
    bool IsFull3D() const { return Stereoscopic3D() && (2==full3D_); }

    // set fullpath(abs. path) and filename(short file name) are utf-8 encoded
    bool SetFilePath(wchar_t const* wfullpath,
                     char const* fullpath, int full_bytes,
                     char const* filename, int short_bytes);

    // set live stream
    // name : description
    // url  : stream url not the webpage url
    // timeout : the replay time when lag timeout milliseconds
    bool SetLiveStream(char const* name, char const* url, uint32 timeout=1000);
};

class VRVideoConfig {
    struct Media {
        uint32 name; // pos
        uint32 url;  // pos
        uint32 sv3d; // 0:2D or N/A,
                     // 1:180, 2:180+sbs, 3:180+tb
                     // 4:360, 5:360+sbs, 6:360+tb
                     // 7:sbs, 8:tb
        uint32 sa3d; // 1:fuma, 2:ambiX, 0:not specified
        uint32 timeout;
    };

    mlabs::balai::StringPool stringPool_;
    mlabs::balai::Array<Media> liveStreams_;
    mlabs::balai::Array<Media> videoPaths_;
    mlabs::balai::Array<Media> videos_;

    int fails_;

    // not define
    VRVideoConfig();
    VRVideoConfig(VRVideoConfig const&);
    VRVideoConfig& operator=(VRVideoConfig const&);

public:
    explicit VRVideoConfig(wchar_t const* xml);

    bool AddLiveStream(char const* name, char const* url, uint32 sv3d, uint32 sa3d, uint32 timeout) {
        if (NULL!=name && NULL!=url) {
            uint32 const crc = stringPool_.HashString(url);
            if (!stringPool_.Find(crc)) {
                Media s;
                s.name = stringPool_.AddString(name);
                s.url = stringPool_.AddString(url);
                s.sv3d = sv3d;
                s.sa3d = sa3d;
                s.timeout = timeout;
                try {
                    liveStreams_.push_back(s);
                    return true;
                }
                catch (...) {
                    return false;
                }
            }
        }
        return false;
    }
    bool AddVideoPath(char const* name, char const* url) {
        if (NULL!=url) {
            uint32 const crc = stringPool_.HashString(url);
            if (!stringPool_.Find(crc)) {
                Media s;
                s.name = (NULL!=name) ? stringPool_.AddString(name):0;
                s.url = stringPool_.AddString(url);
                try {
                    videoPaths_.push_back(s);
                    return true;
                }
                catch (...) {
                    return false;
                }
            }
        }
        return false;
    }
    bool AddVideo(char const* name, char const* url, uint32 sv3d, uint32 sa3d) {
        if (NULL!=url) {
            uint32 const crc = stringPool_.HashString(url);
            if (!stringPool_.Find(crc)) {
                Media s;
                s.name = (NULL!=name) ? stringPool_.AddString(name):0;
                s.url = stringPool_.AddString(url);
                s.sv3d = sv3d;
                s.sa3d = sa3d;
                try {
                    videos_.push_back(s);
                    return true;
                }
                catch (...) {
                    return false;
                }
            }
        }
        return false;
    }

    uint32 GetTotalLiveStreams() const { return (0==fails_) ? liveStreams_.size():0; }
    uint32 GetTotalVideoPaths() const { return (0==fails_) ? videoPaths_.size():0; }
    uint32 GetTotalVideos() const { return (0==fails_) ? videos_.size():0; }
    bool GetLiveStreamByIndex(uint32 id, char const*& name, char const*& url, uint32& sv3d, uint32& sa3d, uint32& timeout) const {
        name = url = NULL;
        if (id<liveStreams_.size()) {
            Media const& s = liveStreams_[id];
            name = stringPool_.Lookup(s.name);
            url = stringPool_.Lookup(s.url);
            sv3d = s.sv3d;
            sa3d = s.sa3d;
            timeout = s.timeout;
        }
        return NULL!=name && NULL!=url;
    }
    bool GetVideoPathByIndex(uint32 id, char const*& name, char const*& url) const {
        name = url = NULL;
        if (id<videoPaths_.size()) {
            Media const& s = videoPaths_[id];
            url = stringPool_.Lookup(s.url);
            if (s.name) {
                name = stringPool_.Lookup(s.name);
            }
            else {
                name = "noname"; // not a big deal
            }
        }
        return NULL!=name && NULL!=url;
    }
    bool GetVideoByIndex(uint32 id, char const*& name, char const*& url, uint32& sv3d, uint32& sa3d) const {
        name = url = NULL;
        if (id<videos_.size()) {
            Media const& s = videos_[id];
            url = stringPool_.Lookup(s.url);
            sv3d = s.sv3d;
            sa3d = s.sa3d;
            if (NULL!=url) {
                if (s.name) {
                    name = stringPool_.Lookup(s.name);
                }
                else {
                    name = strrchr(url, '/');
                    char const* name2 = strrchr(url, '\\');
                    if (name<name2) {
                        name = ++name2;
                    }
                    else if (NULL==name++) {
                        name = url;
                    }
                }
            }
        }
        return NULL!=name && NULL!=url;
    }
};

class VRVideoPlayer : public IAVDecoderHost
{
    enum {
        // let's have 4x3 videos, each video for 4:3 aspect ratio,
        // it ends up a 16:9 panel, great!
        MENU_VIDEO_THUMBNAIL_COLUMN = 4,
        MENU_VIDEO_THUMBNAIL_ROW = 3,

        // video preview in one page
        MENU_VIDEO_THUMBNAILS = MENU_VIDEO_THUMBNAIL_COLUMN*MENU_VIDEO_THUMBNAIL_ROW,

        // preload 3 (previous, current, and next) pages video thumbnail (maximum).
        MENU_VIDEO_THUMBNAIL_TEXTURES = MENU_VIDEO_THUMBNAILS*3,

        // video thumbnail size. aspect ratio = 4:3, a video thumbnail texture contains
        // 4 thumbnails, 2 rows x 2 columns
        VIDEO_THUMBNAIL_WIDTH  = 512,
        VIDEO_THUMBNAIL_HEIGHT = 384,
        VIDEO_THUMBNAIL_TEXTURE_WIDTH = VIDEO_THUMBNAIL_WIDTH * 2,
        VIDEO_THUMBNAIL_TEXTURE_HEIGHT = VIDEO_THUMBNAIL_HEIGHT * 2,

        // UI Font renderer
        UI_TEXT_FONT_SIZE = 160, // in pixels
        UI_TEXT_FONT_DISTANCE_SPREAD_FACTOR = 16,//UI_TEXT_FONT_SIZE/4,
        UI_TEXT_FONT_PYRAMID = 2, // downsizing (mipmap level, 0 for 1:1, 1 for 1:2...)

        // multiple language streams support
        MAX_LANGUAGE_STREAMS = 64,

        // 2016.08.31 dim background
        DIM_BACKGROUND_CONTROLS = 4,
    };

    // text & UI
    enum DRAW_GLYPH {
        DRAW_TEXT_VIDEO_PATH,
        DRAW_TEXT_PUT_VIDEOS_IN_PATH,
        DRAW_TEXT_NO_VIDEOS, // no videos
        DRAW_TEXT_APP_WILL_QUIT,
        DRAW_TEXT_WARNING,
        DRAW_TEXT_ERROR,
        DRAW_TEXT_MENU, // widget to get back to main menu
        DRAW_TEXT_EXIT,
        DRAW_TEXT_NEXT,

        DRAW_TEXT_3D,
        DRAW_TEXT_LR, // SBS or side by side.
        DRAW_TEXT_TB, // Top-Bottom

        DRAW_TEXT_VR,
        DRAW_TEXT_360,
        DRAW_TEXT_180,

        DRAW_TEXT_TRACK_NO, // "Track#"
        DRAW_TEXT_SUBTITLE_DISABLE, // "Subtitle Off"

        DRAW_TEXT_720P,   // HD
        DRAW_TEXT_1080P,  // Full HD
        DRAW_TEXT_4K,     // 4K UHD
        DRAW_TEXT_8K,     // 8K UHD
        DRAW_TEXT_COLON,  // ":"
        DRAW_TEXT_SLASH,  // "/"
        DRAW_TEXT_0,      // "0"
        DRAW_TEXT_1,      // "1"
        DRAW_TEXT_2,      // "2"
        DRAW_TEXT_3,      // "3"
        DRAW_TEXT_4,      // "4"
        DRAW_TEXT_5,      // "5"
        DRAW_TEXT_6,      // "6"
        DRAW_TEXT_7,      // "7"
        DRAW_TEXT_8,      // "8"
        DRAW_TEXT_9,      // "9"

        DRAW_TEXT_TOTALS,

        DRAW_UI_MENU = DRAW_TEXT_TOTALS, // backMenu.png by yumei
        DRAW_UI_CIRCLE,     // circle.png by yumei

            // 2016.08.30
            DRAW_UI_LIGHT,    // light.png by yumei
            DRAW_UI_LANGUAGE, // audio.png by yumei
            DRAW_UI_SUBTITLE, // subtitle.png by yumei
            DRAW_UI_BUSY,     // busy.png by yumei

        DRAW_UI_NEXT,       // nextTrack.png by yumei
        DRAW_UI_PREV,       // mirrow of nextTrack.png
        DRAW_UI_PLAY,       // playIcon.png
        DRAW_UI_PAUSE,      // pauseIcon.png
        DRAW_UI_REPLAY,     // repeat.png by yumei
        DRAW_UI_VOLUME,     // volume.png by yumei
        DRAW_UI_MUTE,       // mute.png by yumei
        DRAW_UI_SETTINGS,   // settings.png by yumei
        DRAW_UI_SWITCH_BASE,   // switch_base.png by yumei
        DRAW_UI_SWITCH_BUTTON, // switch_button.png by yumei

        DRAW_UI_CONFIG_RECT,     // no texture yet
        DRAW_UI_SWITCH_3D,       // place holder, no texture
        DRAW_UI_SWITCH_3D_SBS,   // place holder, no texture
        DRAW_UI_SWITCH_3D_TB,    // place holder, no texture
        DRAW_UI_SWITCH_VR,       // place holder, no texture
        DRAW_UI_SWITCH_VR_ANGLE, // place holder, no texture

        DRAW_UI_VOLUME_BAR, // no texture yet
        DRAW_UI_SEEK_BAR,   // no texture yet

        DRAW_GLYPH_TOTALS,
    };

    static mlabs::balai::graphics::Color const viveColor_;

    // menu popup time
    static float const menu_animation_duration_;

    // generous animation duration
    static float const animation_duration_;

    std::thread thumbnailThread_;
    std::condition_variable pageCV_, thumbnailCV_;
    std::mutex  thumbnailMutex_;

    // video thumbnail texture in menu, will preload 3 pages maximum -
    //  previous page, current page and next page. (but no more than total videos)
    wchar_t video_path_[256]; // full path name
    mlabs::balai::graphics::Texture2D* videoThumbnails_[MENU_VIDEO_THUMBNAIL_TEXTURES];
    struct Rectangle {
        float x0, z0; // top-left corner
        float x1, z1; // bottom-right corner, z0>z1
        Rectangle():x0(0.0f),z0(0.0f),x1(0.0f),z1(0.0f) {}
        bool In(float x, float z) const { return x0<x && x<x1 && z1<z && z<z0; }
        float AspectRatio() const { return (x1-x0)/(z0-z1); }
        float Width() const { return (x1-x0); }
        float Height() const { return (z0-z1); }
        void Move(float dx, float dz) { x0+=dx; x1+=dx; z1+=dz; z0+=dz; }
        void Extend(float sx, float sz) {
            float const x = 0.5f*(x0+x1);
            float const z = 0.5f*(z0+z1);
            sx = sx*(x-x0); x0 = x - sx; x1 = x + sx;
            sz = sz*(z0-z); z0 = z + sz; z1 = z - sz;
        }
    } thumbnailRectangles_[MENU_VIDEO_THUMBNAILS];
    int reference_tracks_[MENU_VIDEO_THUMBNAIL_TEXTURES];
    std::atomic<int> publishingThumbnailId_;
    FILE* thumbnail_filecache_;

    // video decoder
    AVDecoder decoder_;

    // audio manager
    AudioManager& audioManager_;

    // master volume
    MasterVolumeKeeper masterVolume_;

    // texture coordinate:
    //  refer fonts_[0] to draw text,
    //  refer uiGlyph_ to draw UI glyph
    TexCoord texcoords_[DRAW_GLYPH_TOTALS+ISO_639_TOTALS];

    // font textures
    mlabs::balai::Array<mlabs::balai::graphics::ITexture*> fonts_;

    // playlist
    mlabs::balai::Array<VideoTrack*> playList_;
    VideoTrack const* current_track_; // if NULL, we're in menu mode.

    // multilanguage
    TexCoord extSubtitleFilename_[MAX_LANGUAGE_STREAMS];
    ISO_639 subtitles_[MAX_LANGUAGE_STREAMS];
    ISO_639 languages_[MAX_LANGUAGE_STREAMS];
    int     subStmIDs_[MAX_LANGUAGE_STREAMS];
    int     lanStmIDs_[MAX_LANGUAGE_STREAMS];
    mlabs::balai::graphics::Texture2D* extSubtitleInfo_;

    //
    // flag when system is busy. 2016.09.09
    std::atomic<uint32> on_processing_;
    //
    //  0x1000 : loading video file
    //  0x1001 : failed to load file
    //  0x1002 : video loaded, play it!
    //  0x1004 : try playing
    //     play failed need to take care?
    //  0x1005 : live stream timeout, restart
    //
    //  0x2000 : replay(stop, rewind and play)
    //
    //  0x8000 : start seeking
    //  0x8002 : seek frame
    //  0x8004 : on seek end
    //
    float on_processing_time_start_;

    // graphics objects
    mlabs::balai::graphics::ShaderEffect* fontFx_; // simple font rendering shader, less aliasing
    mlabs::balai::graphics::ShaderEffect* subtitleFx_;
    mlabs::balai::graphics::shader::Constant const* subtitleFxCoeff_;
    mlabs::balai::graphics::shader::Constant const* subtitleFxShadowColor_;
    mlabs::balai::graphics::ShaderEffect* pan360_; // how could you not fall in love with pan360?
    mlabs::balai::graphics::shader::Constant const* pan360Crop_;
    mlabs::balai::graphics::shader::Constant const* pan360Diffuse_;
    mlabs::balai::graphics::shader::Sampler const* pan360Map_;
    mlabs::balai::graphics::ShaderEffect* pan360NV12_;
    mlabs::balai::graphics::shader::Constant const* pan360NV12Crop_;
    mlabs::balai::graphics::shader::Sampler const* pan360MapY_;
    mlabs::balai::graphics::shader::Sampler const* pan360MapUV_;
    mlabs::balai::graphics::ShaderEffect* videoNV12_;
    mlabs::balai::graphics::shader::Sampler const* videoMapY_;
    mlabs::balai::graphics::shader::Sampler const* videoMapUV_;
    mlabs::balai::graphics::Texture2D* videoY_;
    mlabs::balai::graphics::Texture2D* videoUV_;
    mlabs::balai::graphics::Texture2D* subtitle_;
    mlabs::balai::graphics::Texture2D* uiGlyph_;
    mlabs::balai::graphics::ITexture*  background_;

    // VR manager
    mlabs::balai::VR::Manager& vrMgr_;

    // current focus controller, focus_controller_ may be changed
    mlabs::balai::VR::TrackedDevice const* focus_controller_;

    // buffers
    uint8* thumbnailBuffer_;
    uint8* audioBuffer_;
    int    thumbnailBufferSize_;
    int    audioBufferSize_;

    // sphere geometry
    struct SphereGeometry {
        enum { NUM_LONGITUDES = 24, NUM_LATITUDES = 12 };
        GLuint  vao_, vbo_, ibo_;
        GLsizei num_indices_; // #(24, 12) = 589 tristrip
        int longitude_, latitude_south_, latitude_north_;
        SphereGeometry():vao_(0),vbo_(0),ibo_(0),num_indices_(0),
            longitude_(0),latitude_south_(0),latitude_north_(0) {}
        ~SphereGeometry() { Destroy(); }
        bool Create(int longitude=360, int latitude_south=-90, int latitude_north=90);
        void Destroy() {
            if (vao_) {
                glDeleteVertexArrays(1, &vao_);
            }
            if (vbo_) {
                glDeleteBuffers(1, &vbo_);
            }
            if (ibo_) {
                glDeleteBuffers(1, &ibo_);
            }
            vao_ = vbo_ = ibo_ = 0; num_indices_ = 0;
            longitude_ = latitude_south_ = latitude_north_ = 0;
        }
        bool Drawcall() const {
            if (vao_ && num_indices_>0) {
                glBindVertexArray(vao_);
                glDrawElements(GL_TRIANGLE_STRIP, num_indices_, GL_UNSIGNED_SHORT, 0);
                //glDrawElements(GL_LINE_STRIP, num_indices_, GL_UNSIGNED_SHORT, 0);
                return true;
            }
            return false;
        }
    } fullsphere_, customized_;

    // HMD orientation
    mlabs::balai::math::Matrix3 hmd_xform_;
    // 360 video viewer/listener orientation (azimuth_adjust_ applied)
    mlabs::balai::math::Matrix3 viewer_xform_;
    // plane video/widget orientation
    mlabs::balai::math::Matrix3 dashboard_xform_;
    // text (video filename and subtitle) rendering transform
    mutable mlabs::balai::math::Matrix3 text_xform_;

    // dashboard/plane video play area
    float dashboard_distance_;
    float dashboard_width_; // in meters
    float dashboard_height_;
    float dashboard_hit_x_; // in dashboard local space
    float dashboard_hit_z_; // in dashboard local space
    float dashboard_hit_dist_; // focus controller picking distance. 100m = very far
    //float screen_width_;  // screen size <= dashboard size
    //float screen_height_; // with same aspect ratio as video
    float widget_left_;
    float widget_top_;
    float widget_width_;
    float widget_height_;

    // to center 360 video plus user control(trigger+swipe)
    float azimuth_adjust_;

    // subtitle
    SubtitleRect subtitle_rects_[Subtitle::MAX_NUM_SUBTITLE_RECTS];
    int subtitle_rect_count_;
    int subtitle_pts_, subtitle_duration_; // milliseconds

    // event time stamps
    float event_timestamp_;
    float subevent_timestamp_;
    float widget_timer_;

    // main menu page [0, playList_.size()/MENU_VIDEO_THUMBNAILS + 1]
    // (menu mode if current_track_ = NULL)
    float menu_page_scroll_; // page scroll animation, range : in (-1.0, +1.0)
    std::atomic<int> menu_page_;
    int menu_picking_slot_;

    // quit app if quit_count_down_ to 0, e.g.
    // set to 90*10 if you want to quit app after 10 seconds.
    int quit_count_down_;

    // 2016.08.31 dim background
    int dim_bkg_;
    int total_subtitle_streams_;
    int total_audio_streams_; // language

    // controller interaction
    uint8 menu_auto_scroll_;
    uint8 trigger_dashboard_;
    uint8 trigger_dashboard_align_;
    uint8 widget_on_; //
                      // video player widget:
                      //   0 = off(widget closed)
                      //   1 = fade(time) out(opening/closing)
                      //   2 = fade in(and full opened)
                      //     3 = touch seek bar
                      //     4 = leave seek bar
                      //     5 = seeking(pause mode)
                      //     6 = seeking(playing mode)
                      //   7 = touch replay
                      //     8 = touch play/pause
                      //     9 = trigger play/pause
                      //  10 = touch next video
                      //    11 = touch volume bar
                      //    12 = leave volume bar
                      //  13 = touch light
                      //  14 = touch menu
                      //    15 = on language menu
                      //    16 = language changed
                      //  17 = on subtitle menu
                      //  18 = subtitle changed
                      //
                      // video setting(menu mode):
                      //   0 = setting off
                      //   1 = fade(time) out
                      //   2 = fade in
                      //   3 = toggle 3D
                      //   
                      //   5 = VR off <-> 360
                      //   6 = 360 <-> 180
                      //   7 = VR off <-> 180
                      //
    uint8 loop_mode_;
        // pad 3 bytes

    // playback a track - render thread
    bool Playback_(VideoTrack* track);

    // setup dashboard pose (and text out transfom) with respect to HMD
    bool AlignSubtitle360Transform_(mlabs::balai::math::Matrix3& xform) const;
    bool ResetDashboardPose_();

    // connect focus controller. focus_controller_ may change after return.
    void ConnectFocusController_();

    // pick test, return distance>0.0 if hit. hit_x, hit_z is the pick location in xz-plane
    float DashboardPickTest_(float& hit_x, float& hit_z,
                             mlabs::balai::VR::TrackedDevice const* controller) const {
        float hit_dist = PickTest(hit_x, hit_z, dashboard_xform_, controller);
        if (0.0f<hit_dist) {
            float const dw = 0.5f*dashboard_width_;
            float const dh = 0.5f*dashboard_height_;
            if (-dw<hit_x && hit_x<dw && -dh<hit_z && hit_z<dh)
                return hit_dist;
        }
        return -1.0f;
    }

    // return thumbnail id, valid range in [0, 11]
    int DashboardPickThumbnail_(mlabs::balai::VR::TrackedDevice const* controller) const {
        float hit_x, hit_z;
        if (0.0f<DashboardPickTest_(hit_x, hit_z, controller)) {
            for (int row=0; row<MENU_VIDEO_THUMBNAIL_ROW; ++row) {
                Rectangle const* rect = thumbnailRectangles_ + row*MENU_VIDEO_THUMBNAIL_COLUMN;
                if (rect->z1<hit_z && hit_z<rect->z0) {
                    for (int col=0; col<MENU_VIDEO_THUMBNAIL_COLUMN; ++col,++rect) {
                        if (rect->x0<hit_x && hit_x<rect->x1) {
                            return row*MENU_VIDEO_THUMBNAIL_COLUMN + col;
                        }
                    }
                }
            }
        }
        return -1;
    }

    int DashboardPickVideoTrack_(mlabs::balai::VR::TrackedDevice const* controller) const {
        int const tid = DashboardPickThumbnail_(controller);
        if (0<=tid) {
            int const vid = menu_page_*MENU_VIDEO_THUMBNAILS + tid;
            if (vid<(int)playList_.size()) {
                return vid;
            }
        }
        return -1;
    }

    // [render thread] clear video texture...
    void ClearVideoTexture_() {
        if (videoY_ && videoUV_) {
            // black pixel : Y=0, U=128 V=128(Y=U=V=0 is greenish color)
            // (size doesn't matter)
            uint16 const w = 1280;
            uint16 const h = 720;
            uint8 blackYUV[w*h];
            memset(blackYUV, 0x10, w*h); // Rec/BT.709  Y:[16, 235]
            videoY_->UpdateImage(w, h, mlabs::balai::graphics::FORMAT_I8, blackYUV);
            memset(blackYUV, 0x80, w*h/2);
            videoUV_->UpdateImage(w/2, h/2, mlabs::balai::graphics::FORMAT_IA8, blackYUV);
        }
    }

    // VR interrupt handler
    void VRInterrupt_(mlabs::balai::VR::VR_INTERRUPT e, void*) {
        switch(e)
        {
        case mlabs::balai::VR::VR_INTERRUPT_PAUSE:
            if (NULL!=current_track_) { // playing mode
                std::async([this] { decoder_.Stop(); });
                if (widget_on_<2) {
                    widget_on_ = 2;
                    widget_timer_ = (float) mlabs::balai::system::GetTime();
                    if (current_track_->IsSpherical()) {
                        ResetDashboardPose_();
                    }
                }
            }
            break;

        case mlabs::balai::VR::VR_INTERRUPT_RESUME:
            //
            // 2016/06/03
            // a bug report from forum... user want to disconnect controllers to save battery life.
            // https://www.htcvive.com/cn/forum/chat.php?mod=viewthread&tid=873&extra=&page=3
            // #21
            if (NULL!=current_track_ && 0==(0x1000&on_processing_)) {
                if (!decoder_.IsPlaying()) { // not necessary!?
                    std::async([this] { decoder_.Play(); });
                }
            }
            break;

        case mlabs::balai::VR::VR_QUIT:
            quit_count_down_ = 1; // quit at once
            break;
        }
    }

    // audio render callback
    bool RenderAudioData_(uint8* stream, int size, AudioConfig const& config) {
        assert(stream && size>0 && config);
        //
        // TO-DO : apply damper on listener to avid drastic changes...
        //
        mlabs::balai::math::Matrix3 const listener = viewer_xform_;

        AudioInfo aif;
        float gain = 1.0f;
        int const bytes_per_frames = config.BytesPerFrame();
        int const total_frames = size/bytes_per_frames;
        int frames_get = decoder_.StreamingAudio(audioBuffer_, audioBufferSize_, total_frames, gain, &aif);
        if (frames_get<total_frames) {
            // if shorts of some samples, it may because audio buffer is too small...
            int const frame_size = aif.TotalChannels*BytesPerSample(aif.Format);
            int const buffer_size = total_frames*frame_size;
            if (audioBufferSize_<buffer_size) {
                uint8* buffer = (uint8*) blMalloc(buffer_size);
                if (NULL!=buffer) {
                    if (audioBuffer_) {
                        if (0<frames_get) {
                            memcpy(buffer, audioBuffer_, frames_get*frame_size);
                        }
                        blFree(audioBuffer_);
                    }
                    audioBuffer_ = buffer;
                    audioBufferSize_ = buffer_size;
                }
            }

            // keep fetching more samples until it gets enough (or it can't).
            float gain2 = 1.0f;
            while (frames_get<total_frames) {
                int const size_offset = frames_get*frame_size;
                int const gets = decoder_.StreamingAudio(audioBuffer_ + size_offset,
                                                         audioBufferSize_ - size_offset,
                                                         total_frames - frames_get,
                                                         gain2, NULL);
                if (gets>0) {
                    frames_get += gets;
                }
                else {
                    break;
                }
            }
        }

        if (frames_get>0) {
            // translate audio desc
            AudioDesc desc;
            desc.Technique = aif.Technique;
            desc.Format = aif.Format;
            desc.SampleRate = aif.SampleRate;
            desc.NumChannels = aif.TotalChannels;
            desc.NumTracks = aif.TotalStreams;
            //desc.Indices[16]; not use?

            audioManager_.DecodeAudioData(stream, config, audioBuffer_, frames_get, desc, listener, gain);
            size -= frames_get*bytes_per_frames;
            stream += frames_get*bytes_per_frames;
        }

        // fill silence samples if required(probably because video is ended?)
        if (0<size) {
            memset(stream, 0, size);
        }

        return true;
    }

    // thumbnail rectangle(neutral position without menu_page_scroll_)
    void CalcDashboardThumbnailRect_();

    // draw text(video file name)
    void DrawText_(mlabs::balai::math::Matrix3 const& xform,
                   mlabs::balai::graphics::ITexture* font, float font_size,
                   mlabs::balai::graphics::Color const& color, TexCoord const& tc) const;

    // draw subtitles 2016.11.11
    void DrawSubtitle_(mlabs::balai::math::Matrix3 const& xform, int timestamp,
                       Rectangle const& screen, bool clipping, bool is360) const;

    // move dashboard
    void RealignDashboard_(mlabs::balai::VR::TrackedDevice const* ctrl);

    // animate pick rect
    bool OnSelectedThumbnailTransform_(mlabs::balai::math::Matrix3& xform, int slot, float elapsed_time) const;

    // get where the widget should we place it
    bool GetUIWidgetRect_(Rectangle& rect, DRAW_GLYPH g) const;

    // get language and subtitle rect, g=DRAW_UI_LANGUAGE or DRAW_UI_SUBTITLE, item=-1 for base.
    bool GetLanguageSubtitleRect_(Rectangle& rect, DRAW_GLYPH g, int item) const;

    // menu update a branch of FrameMove()
    bool MenuUpdate_(mlabs::balai::VR::TrackedDevice const* prev_ctrl);

    // draw main menu
    bool DrawMainMenu_(mlabs::balai::VR::HMD_EYE eye) const;

    // draw widget
    bool DrawWidgets_(mlabs::balai::VR::HMD_EYE eye) const;

    // loop for thumbnail decoding
    void ThumbnailDecodeLoop_();

    // no copy allowed
    BL_NO_COPY_ALLOW(VRVideoPlayer);

public:
    VRVideoPlayer();
    virtual ~VRVideoPlayer();

    // timing, buffering
    int Duration() const { return decoder_.GetDuration(); }
    int Timestamp() const { return decoder_.Timestamp(); }
    int VideoTime() const { return decoder_.VideoTime(); }
    int AudioTime() const { return decoder_.AudioTime(); }
    bool IsPlaying() const { return decoder_.IsPlaying(); }
    bool IsLoaded() const { return decoder_.IsLoaded(); }
    bool IsEnded() const { return decoder_.NearlyEnd(); }
    int VideoBuffering(int& packets, int& sn) const {
        return decoder_.VideoBuffering(packets, sn);
    }
    int AudioBuffering(int& packets, int& sn) const {
        return decoder_.AudioBuffering(packets, sn);
    }
    int SubtitleBuffering(int& extStreamId, bool& hardsub, int& start, int& duration) const {
        return decoder_.SubtitleBuffering(extStreamId, hardsub, start, duration);
    }
    bool ToggleStartStop() {
        if (0==(0x1000&on_processing_) && !playList_.empty()) {
            if (NULL!=current_track_) {
                if (!decoder_.IsPlaying()) {
                    //int const t = decoder_.VideoTime() - 500;
                    //if (decoder_.PlayAt(t>0 ? t:0)) {
                    if (decoder_.Play()) {
                        if (2==widget_on_) {
                            widget_on_ = 1;
                            widget_timer_ = (float) mlabs::balai::system::GetTime();
                        }
                        return true;
                    }
                }
                else if (decoder_.Stop()) {
                    if (widget_on_<2) {
                        widget_on_ = 2;
                        widget_timer_ = (float) mlabs::balai::system::GetTime();
                        if (current_track_->IsSpherical()) {
                            ResetDashboardPose_();
                        }
                    }
                    return true;
                }
            }
            else {
                return Playback_(playList_[0]);
            }
        }
        return false;
    }

#ifndef HTC_VIVEPORT_RELEASE
    void ToggleAudioManagerDebug() { audioManager_.DebugToggle(); }
    void ToggleAudioChange() {
        static int audio_index = 3;
        if (total_audio_streams_>1 && NULL!=current_track_) {
#if 1
            int aid = (audio_index+rand()+1)%total_audio_streams_;
            audio_index = (audio_index!=aid) ? aid:((aid+1)%total_audio_streams_);
            aid = lanStmIDs_[audio_index];
            current_track_->SetAudioStreamID(aid);
            decoder_.SetAudioStream(aid);
#else
            // trigger angry bird audio change crash
            int const aid = (3==audio_index) ? 1:3;
            current_track_->SetAudioStreamID(aid);
            decoder_.SetAudioStream(aid);
            audio_index = aid;
#endif
        }
    }
    void ToggleSubtitleChange() {
        static int subtitle_index = 0;
        if (total_subtitle_streams_>1 && NULL!=current_track_) {
            int sid = (subtitle_index+rand()+1)%total_subtitle_streams_;
            subtitle_index = (sid!=subtitle_index) ? sid:((sid+1)%total_subtitle_streams_);
            sid = subStmIDs_[subtitle_index];
            current_track_->SetSubtitleStreamID(sid);
            decoder_.SetSubtitleStream(sid);
        }
    }
#endif

    void Replay() { if (NULL!=current_track_ && 0==on_processing_) decoder_.Replay(); }
    void PlaySeek(int deltaTime=-10000) {
        if (NULL!=current_track_ && 0==on_processing_ ) {
            int const t = decoder_.VideoTime() + deltaTime;
            decoder_.PlayAt(t>0 ? t:0);
        }
    }
    void PlayAtEnd(int remaintime=10000) {
        if (NULL!=current_track_ && 0==on_processing_) {
            int const t = decoder_.GetDuration() - remaintime;
            decoder_.PlayAt(t>0 ? t:0);
        }
    }
    bool NextTrack(bool reset=false) {
        if (0!=on_processing_)
            return false;
        int const totals = playList_.size();
        if (NULL!=current_track_) {
            for (int i=0; i<totals; ++i) {
                if (playList_[i]==current_track_) {
                    int const id = (i+1)%totals;
                    VideoTrack* track = playList_[id];
                    if (track && reset) {
                        track->SetTimestamp(0);
                    }
                    Playback_(track);
                    int const pageNo = id/MENU_VIDEO_THUMBNAILS;
                    if (pageNo!=menu_page_) {
                        menu_page_ = pageNo;
                        pageCV_.notify_one();
                    }
                    return true;
                }
            }
        }
        return totals>0 && Playback_(playList_[0]);
    }

    bool PrevTrack() {
        if (0!=on_processing_)
            return false;
        int const totals = playList_.size();
        if (NULL!=current_track_) {
            for (int i=0; i<totals; ++i) {
                if (playList_[i]==current_track_) {
                    int const id = (i+totals-1)%totals;
                    Playback_(playList_[id]);
                    int const pageNo = id/MENU_VIDEO_THUMBNAILS;
                    if (pageNo!=menu_page_) {
                        menu_page_ = pageNo;
                        pageCV_.notify_one();
                    }
                    return true;
                }
            }
        }
        return totals>0 && Playback_(playList_[totals-1]);
    }

    // video width and height
    bool GetVideoWidthHeight(int& w, int &h) const {
        w = decoder_.VideoWidth();
        h = decoder_.VideoHeight();
        return w>0 && h>0;
    }

    // video frame rate
    float VideoFrameRate() const { return decoder_.VideoFrameRate(); }

    // set media path - return possible number of media files(render thread)
    int SetMediaPath(wchar_t const* path=L"./video");
    int GetNumTotalVideos() const { return (int) playList_.size(); }

    // Initialize()/Finalize()/FrameUpdate()/FrameMove()/Render() run on render thread
    bool Initialize();
    void Finalize();
    bool FrameMove();
    bool Render(mlabs::balai::VR::HMD_EYE eye) const;

    // IAVDecoderHost overwrite
    bool FrameUpdate(int /*decoder_id*/, void const* nv12, int /*frameId*/, int w, int h) {
        if (NULL!=nv12 && NULL!=videoY_ && NULL!=videoUV_ && w>1 && h>1) {
            //
            // TO-DO : for spherical videos, we can still update only subrect if viewing angle is known...
            //
            videoY_->UpdateImage((uint16)w, (uint16)h, mlabs::balai::graphics::FORMAT_I8, nv12);
            videoUV_->UpdateImage((uint16)w/2, (uint16)h/2, mlabs::balai::graphics::FORMAT_IA8,
                                  ((uint8 const*)nv12)+w*h);
            return true;
        }
        return false;
    }
    bool SubtitleUpdate(int /*decoder_id*/,
                        void const* pixels, int w, int h, int channels,
                        int pts, int duration, int /*subtitleID*/,
                        SubtitleRect* rects, int num_rects) {
        if (pixels && NULL!=subtitle_ && w>0 && h>0 && NULL!=rects && num_rects>0) {
            if (IsSpherical()) {
                // this may set too early
                AlignSubtitle360Transform_(text_xform_);
            }

            if (4==channels) {
                assert(1==num_rects);
                subtitle_->UpdateImage((uint16)w, (uint16)h, mlabs::balai::graphics::FORMAT_RGBA8, pixels);
            }
            else if (1==channels) {
                subtitle_->UpdateImage((uint16)w, (uint16)h, mlabs::balai::graphics::FORMAT_I8, pixels);
            }
            //subtitle_->SetAddressMode(mlabs::balai::graphics::ADDRESS_CLAMP,
            //                          mlabs::balai::graphics::ADDRESS_CLAMP);

            subtitle_pts_ = pts;
            subtitle_duration_ = duration;

            if (num_rects<=Subtitle::MAX_NUM_SUBTITLE_RECTS) {
                subtitle_rect_count_ = num_rects;
            }
            else {
                subtitle_rect_count_ = Subtitle::MAX_NUM_SUBTITLE_RECTS;
            }
            memcpy(subtitle_rects_, rects, subtitle_rect_count_*sizeof(SubtitleRect));
        }

        return true;
    }
    void OnStreamEnded(int /*decoder_id*/, int error) {
#ifdef BL_AVDECODER_VERBOSE
        //BL_LOG("** stream ended error=%d\n", error);
#endif
        error = 0;
    }
    int OnStart(int /*decoder_id*/, int /*timestamp*/, int /*duration*/, bool /*video*/, bool audio) {
        if (audio) {
            audioManager_.Resume();
        }
#if 1
        return 0;
#else
        static int calls = 0;
        return (0==(++calls)%21) ? 0:100; // delay 2 secs test
#endif
    }
    void OnStopped(int /*decoder_id*/, int endtime, int duration, bool theEnd) {
        audioManager_.Pause();
        if (NULL!=current_track_ && !current_track_->IsLiveStream() && 
            endtime>0 && duration>0 && theEnd) {
            current_track_->SetVeryEnd(endtime);
        }
    }
    void OnReset(int /*decoder_id*/) {
        if (NULL!=current_track_) {
            if (current_track_->IsLiveStream()) {
                BL_LOG("** replay live stream %s...\n", current_track_->GetName());
                on_processing_ = 0x1006; // trying to play
                on_processing_time_start_ = (float) mlabs::balai::system::GetTime();
                decoder_.Stop();
                std::async([this] {
                    decoder_.Play();
                    if (0x1006==on_processing_) {
                        on_processing_ = 0;
                    }
                });
                return;
            }

            current_track_->SetTimestamp(0);

            int const total_tracks = playList_.size();
            if (1==loop_mode_ || (0!=loop_mode_&&total_tracks<=1) ) {
                std::async([this] {
                    // sleep 3 secs, C++11
                    std::this_thread::sleep_for(std::chrono::seconds(3));
                    decoder_.PlayAt(0);
                });
            }
            else if (2==loop_mode_ || (total_tracks>1 && current_track_!=playList_.back())) {
                NextTrack(true);
            }
            else if (widget_on_<2) { // stop
                widget_on_ = 2;
                widget_timer_ = (float) mlabs::balai::system::GetTime();
                if (current_track_->IsSpherical()) {
                    ResetDashboardPose_();
                }
            }
        }
    }
    void OnDataLag(int /*decoder_id*/, int lagtime) {
        if (0==on_processing_ && NULL!=current_track_ && current_track_->IsLiveStream() &&
            current_track_->IsTimeout(lagtime)) {
            BL_LOG("** %s lag for %dms, restarting video...\n", current_track_->GetName(), lagtime);
            on_processing_ = 0x1005; // trying to play
            on_processing_time_start_ = (float) mlabs::balai::system::GetTime();
            decoder_.Stop();
            std::async([this] {
                decoder_.Play();
                if (0x1005==on_processing_) {
                    on_processing_ = 0;
                }
            });
        }
        else {
        }
    }

    bool AudioSettingCallback(int /*decoder_id*/, AudioInfo& ai) {
        assert(0<ai.TotalChannels);
/*
        //
        // TO-DO : full check and make best guess for technique.
        // 
        int audio_streamId[64];
        int audio_channels[64];
        ISO_639 languages[64];
        int const total_audio_streams = decoder_.GetAudioStreamInfo(audio_streamId,
                                                                    audio_channels,
                                                                    languages, 64);
*/
        // verify technique
        bool verify_technique = true;
        if (AUDIO_TECHNIQUE_AMBIX==ai.Technique || AUDIO_TECHNIQUE_FUMA==ai.Technique) {
            verify_technique = false;
            int const max_order = (AUDIO_TECHNIQUE_AMBIX==ai.Technique) ? 7:3;
            for (int order=1; order<=max_order; ++order) {
                int const channels = (order+1)*(order+1);
                if (channels<=ai.TotalChannels) {
                    if (channels==ai.TotalChannels) {
                        verify_technique = true;
                        break;
                    }
                    else if ((channels+2)==ai.TotalChannels) {
                        // with stereo headlock...
                        // do not accept single stream ambisonics with headlock.
                        // it could be a 5.1 stream.
                        if (1<ai.TotalStreams && ai.TotalStreams<sizeof(ai.StreamIndices) &&
                            2==ai.StreamIndices[ai.TotalStreams-1]) {
                            verify_technique = true;
                        }
                        break;
                    }
                }
                else {
                    break;
                }
            }
        }
        else if (AUDIO_TECHNIQUE_TBE==ai.Technique) {
            verify_technique = false; // 4+4 or 4+4+2
            if (4==ai.StreamChannels[0] && 4==ai.StreamChannels[1]) {
                if ((8==ai.TotalChannels && 2==ai.TotalStreams) ||
                    (10==ai.TotalChannels && 3==ai.TotalStreams && 2==ai.StreamChannels[2])) {
                    verify_technique = true;
                }
            }
        }

        // if fails, default technique
        if (!verify_technique) {
            ai.Technique = AUDIO_TECHNIQUE_DEFAULT;
            ai.TotalStreams = 1;
            ai.TotalChannels = ai.StreamChannels[0];
        }

        AudioConfig desired, result;
        if (AUDIO_TECHNIQUE_DEFAULT!=ai.Technique) {
            ai.Format = AUDIO_FORMAT_F32;
            ai.SampleRate = 48000;
        }
        else {
            ai.TotalStreams = 1;
            ai.TotalChannels = ai.StreamChannels[0];
            if (6==ai.TotalChannels || 8==ai.TotalChannels) { // 5.1 or 6.1
                ai.Format = AUDIO_FORMAT_F32;
                ai.SampleRate = 48000;
            }
            else {
                ai.TotalChannels = ai.StreamChannels[0] = 2; // stereo downmix
                if (AUDIO_FORMAT_UNKNOWN==ai.Format)
                    ai.Format = AUDIO_FORMAT_F32;
            }
        }

        desired.Channels = 2; // all binaural
        desired.Format = ai.Format;
        desired.SampleRate = ai.SampleRate;
        if (0<ai.TotalChannels && audioManager_.OpenAudio(result, desired)) {
/*
            //
            // see bool RenderAudioData_(uint8* stream, int size, AudioConfig const& config)
            //
            // prepare audio buffer - 4096 per batch sample... could be wrong...
            int const buffer_size = 4096*ai.TotalChannels*BytesPerSample(ai.Format);
            if (NULL==audioBuffer_ || audioBufferSize_<buffer_size) {
                uint8* buffer = (uint8*) blMalloc(buffer_size);
                if (NULL!=buffer) {
                    blFree(audioBuffer_);
                    audioBuffer_ = buffer;
                }
                else {
                    BL_ERR("VRVideoPlayer::AudioSettingCallback() - out of memory!!!\n");
                }
            }
*/
            return true;
        }

        ai.Reset();
        return false;
    }

    // quick helper
    bool IsSpherical() const {
        return (NULL!=current_track_) && current_track_->IsSpherical();
    }

    // 2017.09.12 toggle SW/HW video decoder
    int  NumAvailableVideoDecoders() const { return decoder_.NumAvailableVideoDecoders(); }
    int  IsHardwareVideoDecoder() const { return decoder_.IsHardwareVideoDecoder(); }
    bool ToggleHardwareVideoDecoder() { return decoder_.ToggleHardwareVideoDecoder(); }
    char const* VideoDecoderName() const { return decoder_.VideoDecoderName(); } 

    //
    // the ugly!...

    // 2016.10.13 HOT-FIX
    float DrawVideoPathOnScreen(float x, float y, float h, float screen_aspect_ratio,
                                mlabs::balai::graphics::Color const& color, uint32 align) const;
    // debug purpose only!
    mlabs::balai::VR::TrackedDevice const* GetFocusController() const { return focus_controller_; }

    void ToggleSphericalVideoType() {
        if (NULL!=current_track_) {
            // const_cast... pardon me~
            if (current_track_->IsSpherical()) {
                int longi(0), lati_south(0), lati_north(0);
                if (current_track_->GetSphericalAngles(longi, lati_south, lati_north) && 360==longi) {
                    const_cast<VideoTrack*>(current_track_)->TweakVideoType(VIDEO_TYPE_SPHERICAL, 30); // 180
                }
                else {
                    const_cast<VideoTrack*>(current_track_)->TweakVideoType(VIDEO_TYPE_2D, 0);
                }
            }
            else {
               const_cast<VideoTrack*>(current_track_)->TweakVideoType(VIDEO_TYPE_SPHERICAL, 60);
            }
        }
    }
    void ToggleStereoScope() {
        if (NULL!=current_track_) {
            VIDEO_3D mode = current_track_->Stereoscopic3D();
            if (VIDEO_3D_TOPBOTTOM==mode) {
                mode = VIDEO_3D_LEFTRIGHT;
            }
            else if (VIDEO_3D_LEFTRIGHT==mode) {
                mode = VIDEO_3D_MONO;
            }
            else if (VIDEO_3D_BOTTOMTOP==mode) {
                mode = VIDEO_3D_MONO;
            }
            else if (VIDEO_3D_RIGHTLEFT==mode) {
                mode = VIDEO_3D_MONO;
            }
            else { // mono
                mode = VIDEO_3D_TOPBOTTOM;
            }

            // change aspect ratio if plane mode
            if (const_cast<VideoTrack*>(current_track_)->TweakVideo3D(mode) &&
                !current_track_->IsSpherical()) {
                dashboard_width_ = dashboard_height_*current_track_->AspectRatio();
                //screen_width_ = screen_height_*track->AspectRatio();
                widget_width_ = 0.925f*dashboard_width_; // 0.925f*screen_width_;
                widget_left_  = -0.5f*widget_width_;
            }
        }
    }
    char const* SphericalVideoType() const {
        if (NULL!=current_track_) {
            if (current_track_->IsSpherical()) {
                int longi(0), lati_south(0), lati_north(0);
                if (current_track_->GetSphericalAngles(longi, lati_south, lati_north) && 180==longi) {
                    return "180";
                }
                return "360";
            }
            else {
                return "Plane";
            }
        }
        return "N/A";
    }
    char const* Stereo3D() const {
        if (NULL!=current_track_) {
            VIDEO_3D mode = current_track_->Stereoscopic3D();
            if (VIDEO_3D_TOPBOTTOM==mode) {
                return "Top-Bottom";
            }
            else if (VIDEO_3D_LEFTRIGHT==mode) {
                return "Left-Right";
            }
            else if (VIDEO_3D_BOTTOMTOP==mode) {
                return "Bottom-Top";
            }
            else if (VIDEO_3D_RIGHTLEFT==mode) {
                return "Right-Left";
            }
            else {
                return "Mono";
            }
        }
        return "N/A";
    }
    bool ToggleAmbisonicFormat() {
        return decoder_.ToggleAmbisonicFormat();
    }
    char const* GetAudioInfo(AUDIO_TECHNIQUE& tech) const {
        AudioInfo const& ai = decoder_.GetAudioInfo();
        tech = ai.Technique;
        if (AUDIO_TECHNIQUE_AMBIX==ai.Technique) {
            if (4==ai.TotalChannels) {
                return "AmbiX 1st Order";
            }
            else if (6==ai.TotalChannels) {
                return "AmbiX 1st Order + Headlock";
            }
            else if (9==ai.TotalChannels) {
                return "AmbiX 2nd Order";
            }
            else if (11==ai.TotalChannels) {
                return "AmbiX 2nd Order + Headlock";
            }
            else if (16==ai.TotalChannels) {
                return "AmbiX 3rd Order";
            }
            else if (18==ai.TotalChannels) {
                return "AmbiX 3rd Order + Headlock";
            }
        }
        else if (AUDIO_TECHNIQUE_FUMA==ai.Technique) {
            if (4==ai.TotalChannels) {
                return "FuMa 1st Order";
            }
            else if (6==ai.TotalChannels) {
                return "FuMa 1st Order + Headlock";
            }
            else if (9==ai.TotalChannels) {
                return "FuMa 2nd Order";
            }
            else if (11==ai.TotalChannels) {
                return "FuMa 2nd Order + Headlock";
            }
            else if (16==ai.TotalChannels) {
                return "FuMa 3rd Order";
            }
            else if (18==ai.TotalChannels) {
                return "FuMa 3rd Order + Headlock";
            }
        }
        else if (AUDIO_TECHNIQUE_TBE==ai.Technique) {
            if (8==ai.TotalChannels) {
                return "FB360 TBE";
            }
            else if (10==ai.TotalChannels) {
                return "FB360 TBE + Headlock";
            }
        }
        else {
            if (6==ai.TotalChannels) {
                return "ITU 5.1";
            }
            else if (8==ai.TotalChannels) {
                return "ITU 7.1";
            }
            else if (2==ai.TotalChannels) {
                return "Stereo";
            }
        }
        return NULL;
    }
    bool ToggleLoopMode() {
        int const total_videos = playList_.size();
        if (total_videos>0) {
            if (total_videos>1) {
                loop_mode_ = (loop_mode_+1)%3;
            }
            else {
                loop_mode_ = (0==loop_mode_) ? 1:0;
            }
            return true;
        }
        return false;
    }
    char const* LoopMode() const {
        if (1==loop_mode_) {
            return "One";
        }
        else if (2==loop_mode_) {
            return "All";
        }
        else {
            return "OFF";
        }
    }
};

}
#endif