/**
 *
 * HTC Corporation Proprietary Rights Acknowledgment
 * Copyright (c) 2018
 * All Rights Reserved.
 *
 * The information contained in this work is the exclusive property of HTC Corporation
 * ("HTC").  Only the user who is legally authorized by HTC ("Authorized User") has
 * right to employ this work within the scope of this statement.  Nevertheless, the
 * Authorized User shall not use this work for any purpose other than the purpose
 * agreed by HTC.  Any and all addition or modification to this work shall be
 * unconditionally granted back to HTC and such addition or modification shall be
 * solely owned by HTC.  No right is granted under this statement, including but not
 * limited to, distribution, reproduction, and transmission, except as otherwise
 * provided in this statement.  Any other usage of this work shall be subject to the
 * further written consent of HTC.
 *
 * @file    Pan360.cpp
 * @author  andre chen
 * @history 2018/05/15 created
 *
 */
#include "Pan360.h"
#include "ISO639.h"

#include "uchardet.h"

#include "BLPrimitives.h"
#include "BLJPEG.h"
#include "BLPNG.h"
#include "BLFileStream.h"
#include "BLXML.h"

#include <time.h>

using namespace mlabs::balai;
using namespace mlabs::balai::math;
using namespace mlabs::balai::graphics;
using namespace mlabs::balai::image;

namespace htc {

inline size_t get_abs_path(wchar_t* url, size_t max_len, wchar_t* path, wchar_t const* file) {
    wchar_t const* s = file;
    while (L'.'==*s) {
        if (s[1]==L'.') {
            if (s[2]==L'/') {
                s += 3;
                wchar_t* cc = wcsrchr(path, L'/');
                if (cc) {
                    *cc = L'\0';
                }
                else {
                    if (path[1]==L':') {
                        path[2] = L'\0';
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

    if (*s) {
        if (L':'==s[1] && L'/'==s[2]) {
            return swprintf(url, max_len, L"%s", s);
        }

        return swprintf(url, max_len, L"%s/%s", path, s);
    }
    else {
        size_t len = 0;
        for (; len<max_len&&path[len]; ++len) {
            url[len] = path[len];
        }

        url[len] = L'\0';
        return len;
    }
}

using mlabs::balai::fileio::XML_Element;
using mlabs::balai::fileio::XML_Attrib;

class xmlParser : public mlabs::balai::fileio::XMLParserLite<>
{
public:
    xmlParser() {}
    ~xmlParser() {}

    bool ParseXML(wchar_t const* xml) {
        uint8* buf = NULL;
        uint8* buf_end = NULL;
        if (NULL!=xml) {
            FILE* file = _wfopen(xml, L"rb");
            if (NULL!=file) {
                fseek(file, 0, SEEK_END);
                size_t const file_size = ftell(file);
                rewind(file);
                if (file_size>64) {
                    buf = (uint8*) malloc(file_size);
                    if (NULL!=buf) {
                        if (file_size==fread(buf, 1, file_size, file)) {
                            buf_end = buf + file_size;
                        }
                        else {
                            free(buf);
                            buf = NULL;
                        }
                    }
                }
                fclose(file);
            }
        }

        if (NULL==buf || buf_end<=buf)
            return false;

        uint32 codepage = 0;
        uint8* ptr = buf;
        if (0xEF==ptr[0] && 0xBB==ptr[1] && 0xBF==ptr[2]) {
            codepage = CP_UTF8;
            ptr += 3;
        }
        else if (0xFF==ptr[0] && 0xFE==ptr[1]) {
            if (0x00==ptr[2] && 0x00==ptr[3]) {
                codepage = 12000; // UTF-32 little endian
                ptr += 4;
            }
            else {
                codepage = 1200; // UTF-16 little endian
                ptr += 2;
            }
        }
        else if (0xFE==ptr[0] && 0xFF==ptr[1]) {
            codepage = 1201;// UTF-16 big endian
            ptr += 2;
        }
        else if (0x00==ptr[0] && 0x00==ptr[1] && 0xFE==ptr[2] && 0xFF==ptr[3]) {
            codepage = 12001; // UTF-32 big-endian
            ptr += 4;
        }
        else { // non BOM
            uchardet_t chardet = uchardet_new();
            if (0==uchardet_handle_data(chardet, (char const*) ptr, (int)(buf_end-ptr))) {
                uchardet_data_end(chardet);
                const char* result = uchardet_get_charset(chardet);
                codepage = mlabs::balai::FindCodePage(result);
            }
            uchardet_delete(chardet);
            if (0==codepage) {
                BL_ERR("ucharset Handle error. assume to be utf-8\n");
                codepage = CP_UTF8;
            }
        }

        int const data_length = (int) (buf_end-ptr);
        if (1200==codepage||1201==codepage) {
            int const valid_size = data_length & ~1;
            if (1201==codepage) { // big endian
                uint8 const* dst_end = ptr + valid_size;
                for (uint8* dst=ptr; dst<dst_end; dst+=2) {
                    uint8 t = dst[0];
                    dst[0] = dst[1];
                    dst[1] = t;
                }
            }

            int len = WideCharToMultiByte(CP_UTF8, 0, (wchar_t const*) ptr, valid_size/2, NULL, 0, NULL, NULL);
            if (len>0) {
                char* utf8 = (char*) malloc(len+1);
                int len2 = WideCharToMultiByte(CP_UTF8, 0, (wchar_t const*) ptr, valid_size/2, utf8, len, NULL, NULL);
                if (len==len2) {
                    utf8[len] = '\0';
                    free(buf);
                    ptr = buf = (uint8*) utf8;
                    buf_end = buf + len;
                    codepage = CP_UTF8;
                }
                else {
                    free(utf8);
                }
            }
        }
        else if (CP_UTF8!=codepage) {
            //
            // will this work for utf-32 (12000==codepage||12001==codepage) ???
            // how can you make a utf-32 file anyway!?
            //
            int len = MultiByteToWideChar(codepage, 0, (char const*) ptr, data_length, NULL, NULL);
            if (len>0) {
                wchar_t* wtext = (wchar_t*) malloc((len+1)*sizeof(wchar_t));
                if (wtext) {
                    int len2 = MultiByteToWideChar(codepage, 0, (char const*) ptr, data_length, wtext, len);
                    if (len==len2) {
                        len = WideCharToMultiByte(CP_UTF8, 0, wtext, len2, NULL, 0, NULL, NULL);
                        if (len>0) {
                            char* utf8 = (char*) malloc(len+1);
                            len2 = WideCharToMultiByte(CP_UTF8, 0, wtext, len2, utf8, len, NULL, NULL);
                            if (len==len2) {
                                utf8[len] = '\0';
                                free(buf);
                                ptr = buf = (uint8*) utf8;
                                buf_end = buf + len;
                                codepage = CP_UTF8;
                            }
                            else {
                                free(utf8);
                            }
                        }
                    }
                    free(wtext);
                }
            }
        }

        bool result = false;

        if (CP_UTF8==codepage) {
            char const* xml_header = "<?xml version=\"1.0\" encoding=\"utf-8\"?>"; // length = 38
            char const* xml_begin = (char const*) ptr;
            char const* xml_end = (char const*) buf_end;
            if (0==memcmp(xml_begin, xml_header, 38)) {
                xml_begin += 38;

                while ('<'!=*xml_begin && xml_begin<xml_end)
                    ++xml_begin;

                while ('>'!=xml_end[-1] && xml_begin<xml_end)
                    --xml_end;

                result = xmlParser::Parse(xml_begin, xml_end);
            }
        }

        free(buf);

        return result;
    }
};

class ConfigParser : public xmlParser
{
    char   winTitle_[128];
    char   path_[256];
    uint32 loading_;
    int    winWidth_, winHeight_, fullscreen_;
    int    stacks_;

    bool BeginTag_(XML_Element const& ele, XML_Element const* ascent) {
        static uint32 const crc_pan360 = mlabs::balai::CalcCRC("pan360");
        static uint32 const crc_windows = mlabs::balai::CalcCRC("windows");
        uint32 const tag = mlabs::balai::CalcCRC(ele.Tag);
        if (NULL!=ascent) {
            if (crc_pan360==loading_ && crc_windows==tag) {
                for (int i=0; i<ele.NumAttribs; ++i) {
                    XML_Attrib const& attr = ele.Attributes[i];
                    if (0==memcmp(attr.Name, "title", 6)) {
                        strncpy(winTitle_, attr.Value, sizeof(winTitle_));
                        winTitle_[sizeof(winTitle_)-1] = '\0';
                    }
                    else if (0==memcmp(attr.Name, "width", 6)) {
                        winWidth_ = atol(attr.Value);
                    }
                    else if (0==memcmp(attr.Name, "height", 7)) {
                        winHeight_ = atol(attr.Value);
                    }
                    else if (0==memcmp(attr.Name, "fullscreen", 11)) {
                        if (1==atol(attr.Value) || 0==memcmp(attr.Value, "enable", 7))
                            fullscreen_ = 1;
                    }
                }
            }
        }
        else {
            if (crc_pan360!=tag || 0!=stacks_)
                return false;
            loading_ = tag;
        }

        ++stacks_;
        return true;
    }
    bool EndTag_(XML_Element const& ele, XML_Element const*) {
        if (0==memcmp(ele.Tag, "path", 5)) {
            strncpy(path_, ele.Content, sizeof(path_));
            path_[sizeof(path_)-1] = '\0';
        }

        --stacks_;
        return true;
    }

    BL_NO_COPY_ALLOW(ConfigParser);

public:
    ConfigParser():loading_(0),winWidth_(0),winHeight_(0),fullscreen_(0),stacks_(0) {
        memset(winTitle_, 0, sizeof(winTitle_));
        memset(path_, 0, sizeof(path_));
    }

    // gets...
    int WindowWidth() const { return winWidth_; }
    int WindowHeight() const { return winHeight_; }
    int FullscreenMode() const { return fullscreen_; }
    char const* WindowTitle() const { return winTitle_; }
    char const* TargetPath() const { return path_; }
};

//---------------------------------------------------------------------------------------
Pan360::Pan360():
decodeThread_(),
fontFx_(NULL),
subtitleFx_(NULL),subtitleFxCoeff_(NULL),subtitleFxShadowColor_(NULL),
pan360_(NULL),pan360Crop_(NULL),pan360Map_(NULL),
glyph_(NULL),
equiRect_(NULL),
vrMgr_(mlabs::balai::VR::Manager::GetInstance()),
textBlitter_(),
glyphBuffer_(NULL),
glyphBufferWidth_(0),glyphBufferHeight_(0),
envelope_(),
hmd_xform_(Matrix3::Identity),
glyph_xform_(Matrix3::Identity),
azimuth_adjust_(0.0f),
s3D_(0),
imageMutex_(),
image_pixels_(NULL),
image_width_(0),
image_height_(0),
image_components_(0),
session_id_(-1),image_index_(-1),image_totals_(0),image_loading_(0),image_loadtime_(-10000.0f),
dashboard_interrupt_(0),
quit_count_down_(-1),
lost_tracking_(0)
{
    memset(&filetime_, 0, sizeof(filetime_));
    memset(path_, 0, sizeof(path_));
    memset(image_name_, 0, sizeof(image_name_));
    memset(texture_name_, 0, sizeof(texture_name_));
    memset(glyphRects_, 0, sizeof(glyphRects_));

    // hook interrupt
    vrMgr_.InterruptHandler() = [this] (VR::VR_INTERRUPT e, void* d) {
        VRInterrupt_(e, d);
    };
}
//---------------------------------------------------------------------------------------
void Pan360::AlignGlyphTransform_()
{
    Matrix3 const pose = hmd_xform_;
    Vector3 const Z(0.0f, 0.0f, 1.0f);
    Vector3 Y = pose.YAxis();
    Vector3 X = Y.Cross(Z);
    float xlen = 0.0f;
    X.Normalize(&xlen);
    if (xlen>0.25f) {
        Y = Z.Cross(X);
    }
    else { // Y is up/down
        Y = Z.Cross(pose.XAxis());
        Y.Normalize();
        X = Y.Cross(Z);
    }

    // X
    glyph_xform_._11 = X.x;
    glyph_xform_._21 = X.y;
    glyph_xform_._31 = X.z;

    // Y
    glyph_xform_._12 = Y.x;
    glyph_xform_._22 = Y.y;
    glyph_xform_._32 = Y.z;

    // Z
    glyph_xform_._13 = Z.x;
    glyph_xform_._23 = Z.y;
    glyph_xform_._33 = Z.z;

    // put 1 meter away in front
    glyph_xform_.SetOrigin(pose.Origin());
}
//---------------------------------------------------------------------------------------
bool Pan360::ReadConfig(wchar_t const* xml, char* win_title, int max_win_title_size,
                        int& width, int& height)
{
    ConfigParser config;
    if (config.ParseXML(xml)) {
        if (config.FullscreenMode()) {
            width = height = -1;
        }
        else {
            width = config.WindowWidth();
            height = config.WindowHeight();
        }

        char const* str = config.WindowTitle();
        if (str && str[0]) {
            strncpy(win_title, str, max_win_title_size);
        }

        // path
        wchar_t cwd[MAX_PATH];
        size_t const cwd_len = GetCurrentDirectoryW(MAX_PATH, cwd);
        for (size_t i=0; i<cwd_len; ++i) {
            wchar_t& c = cwd[i];
            if (c==L'\\')
                c = L'/';
        }
        //cwd[cwd_len] = 0;

        str = config.TargetPath();
        if (NULL!=str) {
            wchar_t path[MAX_PATH];
            size_t const path_len = MultiByteToWideChar(CP_UTF8, 0, str, -1, path, MAX_PATH) - 1;
            for (size_t i=0; i<path_len; ++i) {
                wchar_t& c = path[i];
                if (c==L'\\')
                    c = L'/';
            }

            get_abs_path(path_, MAX_PATH, cwd, path);
        }
        else {
            wcsncpy(path_, cwd, MAX_PATH);
        }

        return true;
    }
    return false;
}
//---------------------------------------------------------------------------------------
static void* read_file(int& size, wchar_t const* filename) {
    void* ptr = NULL;
    size = 0;
    if (NULL!=filename) {
        FILE* file = _wfopen(filename, L"rb");
        if (NULL!=file) {
            fseek(file, 0, SEEK_END);
            size_t const file_size = ftell(file);
            rewind(file);
            if (file_size>64) {
                ptr = (uint8*) malloc(file_size);
                if (NULL!=ptr) {
                    size = (int) file_size;
                    if (size!=(int)fread(ptr, 1, file_size, file)) {
                        free(ptr);
                        ptr = NULL;
                        size = 0;
                    }
                }
            }
            fclose(file);
        }
    }
    return ptr;
}

void Pan360::ImageDecodeLoop_()
{
    WIN32_FIND_DATAW fd;
    Array<WIN32_FIND_DATAW> fds_(256);
    wchar_t target[MAX_PATH], filename[MAX_PATH];
    swprintf(target, MAX_PATH, L"%s/*.*", path_);
    void* file_buf = NULL;
    int file_size = 0;
    int cur_session_id = session_id_;
    if (session_id_<0) {
        session_id_ = 0;
        image_index_ = -1; // the newest one
    }

    while (quit_count_down_<0) {
        if (cur_session_id==session_id_) {
            assert(0==image_loading_);
            image_loading_ = 0;
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            continue;
        }

        fds_.clear();

        HANDLE hFind = FindFirstFileW(target, &fd);
        if (INVALID_HANDLE_VALUE!=hFind) {
            do {
                if (0==(FILE_ATTRIBUTE_DIRECTORY&fd.dwFileAttributes)) {
                    int short_len = (int) wcslen(fd.cFileName);
                    if (short_len>4) {
                        wchar_t const* wext = fd.cFileName + short_len - 4;
                        if (*wext++==L'.') {
                            if (0==memcmp(wext, L"jpg", 3*sizeof(wchar_t)) || 0==memcmp(wext, L"JPG", 3*sizeof(wchar_t))) {
                                fds_.push_back(fd);
                            }
                            else if (0==memcmp(wext, L"png", 3*sizeof(wchar_t)) || 0==memcmp(wext, L"PNG", 3*sizeof(wchar_t))) {
                                fds_.push_back(fd);
                            }
                            else if (0==memcmp(wext, L"exr", 3*sizeof(wchar_t)) || 0==memcmp(wext, L"EXR", 3*sizeof(wchar_t))) {
                                fds_.push_back(fd);
                            }
                        }
                    }
                }
            } while (0!=FindNextFileW(hFind, &fd));
            FindClose(hFind);
        }

        int const total_images = (int) fds_.size();
        if (total_images>0) {
            // sort according to what time?
            // ftCreationTime, ftLastAccessTime or ftLastWriteTime?
            fds_.sort([] (WIN32_FIND_DATAW const& a, WIN32_FIND_DATAW const& b) {
                return ::CompareFileTime(&a.ftLastWriteTime, &b.ftLastWriteTime)<0;
            });

            int index = image_index_;
            while (index<0)
                index += total_images;

            index = index%total_images;
            WIN32_FIND_DATAW& f = fds_[index];
            int const short_len = (int) wcslen(f.cFileName);

            //
            // make sure width is multiple of 4 for 3 channels (RGB) images.
            // make sure width is multiple of 4 for 3 channels (RGB) images.
            // make sure width is multiple of 4 for 3 channels (RGB) images.
            //
            void* pixels = NULL;
            int width(0), height(0), channels(0);
            if (0!=memcmp(texture_name_, f.cFileName, (short_len+1)*sizeof(wchar_t)) ||
                (0==f.ftLastWriteTime.dwHighDateTime && 0==f.ftLastWriteTime.dwLowDateTime) ||
                0!=::CompareFileTime(&filetime_, &f.ftLastWriteTime)) {
                swprintf(filename, MAX_PATH, L"%s/%s", path_, f.cFileName);
                file_buf = read_file(file_size, filename);
                if (NULL!=file_buf) {
                    pixels = decompress_JPEG(file_buf, file_size, width, height, channels);
                    if (NULL==pixels) {
                        pixels = decompress_PNG(file_buf, file_size, width, height, channels);
                    }
                    free(file_buf);
                }

                memcpy(&filetime_, &f.ftLastWriteTime, sizeof(filetime_));
            }

            if (NULL!=pixels) {
                std::unique_lock<std::mutex> lock(imageMutex_);
                memcpy(image_name_, f.cFileName, (short_len+1)*sizeof(wchar_t));
                if (NULL!=image_pixels_) free(image_pixels_);
                image_pixels_  = pixels;
                image_width_   = width;
                image_height_  = height;
                image_components_ = channels;
                image_index_   = index;
                image_totals_  = total_images;

                // invalid texture name
                TexCoord& tc = glyphRects_[GLYPH_IMAGE_NAME];
                tc.s1 = tc.s0;
                tc.t1 = tc.t0;
            }
            else {
                // failed!?
            }

            image_loading_ = 0;
        }

        ++cur_session_id;
    }
}
//---------------------------------------------------------------------------------------
bool Pan360::Initialize()
{
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
        "in vec3 t3;\n"
        "layout(location=0) out vec4 c0;\n"
        "void main() {\n"
        "  highp vec3 t = normalize(t3);\n"
        "  t.x = cropf.x + cropf.y*atan(t.x, t.y);\n"
        "  t.y = cropf.z + cropf.w*acos(t.z);\n"
        "  c0 = texture(map, t.xy);\n"
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
    pan360Map_ = pan360_->FindSampler("map");

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

    char const* psh_outline_shadow = "#version 410 core\n"
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
    envelope_.Create();

    // load assets
    image_loading_ = 1;
    int width, height, channels;
    fileio::ifstream fin;
    if (fin.Open("./assets/asset.bin")) {
        fileio::FileHeader header;
        fileio::FileChunk chunk;
        fin >> header;
        if (0==memcmp(header.Description , "The Great Pan360", 16)) {
            uint32 const p360 = BL_MAKE_4CC('P', '3', '6', '0');
            do {
                fin >> chunk;
                switch (chunk.ID)
                {
                case p360:
                    if (NULL==equiRect_) {
                        uint32 const pos0 = fin.GetPosition();
                        int dummy = 0;
                        fin >> width >> height >> channels >> dummy;
                        assert(channels==3);
                        int const data_size = height*width*channels;
                        uint8* pixels = (uint8*) blMalloc(data_size);
                        fin.Read(pixels, data_size);
                        equiRect_ = Texture2D::New(0);
                        equiRect_->UpdateImage((uint16)width, (uint16) height, FORMAT_RGB8, pixels);
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
            } while (!fin.Fail());
        }
    }
#ifndef VRC_FINAL_RELEASE
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
        memcpy(fileHeader.Description, "The Great Pan360", 16);
        stream.BeginFile(fileHeader);

        // Icon
        fileio::FileChunk fc;
        fc.ID = BL_MAKE_4CC('I', 'C', 'O', 'N');
        fc.Version  = 0;
        fc.Elements = 1;
        fc.Size     = 0;
        strcpy(fc.Description, "App Icon RGB565");
        stream.BeginChunk(fc);
        void* icon = mlabs::balai::image::read_JPEG("../VRVideoPlayer/assets/vive_64.jpg", width, height, channels);
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

        // environment pan360 texture
        fc.ID = BL_MAKE_4CC('P', '3', '6', '0');
        fc.Version  = 0;
        fc.Elements = 1;
        fc.Size     = 0;
        strcpy(fc.Description, "Environment 360");
        stream.BeginChunk(fc);
        uint8* pan360 = (uint8*) mlabs::balai::image::read_JPEG("../VRVideoPlayer/assets/viveNight_resize.jpg", width, height, channels);
        stream << (int) width << height << channels << (int) 0;
        stream.WriteBytes(pan360, width*height*channels);
        stream.EndChunk(fc);

        BL_ASSERT(width==4096 && height==2048 && channels==3);
        if (NULL!=equiRect_)
            equiRect_ = Texture2D::New(0);
        equiRect_->UpdateImage((uint16)width, (uint16) height, FORMAT_RGB8, pan360);
        free(pan360);

        stream.EndFile();
    }
#endif

    // start the thread
    decodeThread_ = std::move(std::thread(([this] { ImageDecodeLoop_(); })));

    // text
    memset(glyphRects_, 0, sizeof(glyphRects_));
    char const* font_name = "Calibri"; // to make Korean shown properly
    ISO_639 const lan = ISO_639_ENG;
    int sx(0), sy(0);
    if (textBlitter_.Create(font_name, 160, true, lan, 16) &&
        textBlitter_.GetTextExtend(sx, sy, 2, true, path_, (int)wcslen(path_))) {
        glyphBufferWidth_  = 1024;
        glyphBufferHeight_ =  256;
        while (glyphBufferWidth_<sx) {
            glyphBufferWidth_ *= 2;
        }
        while (glyphBufferHeight_<4*sy) {
            glyphBufferHeight_ *= 2;
        }

        glyphBuffer_ = (uint8*) malloc(glyphBufferWidth_*glyphBufferHeight_); // single channel
        if (glyphBuffer_) {
            memset(glyphBuffer_, 0, sizeof(glyphBuffer_));

            uint8* ptr = glyphBuffer_;
            int ix(0), iy(0);
            if (textBlitter_.DistanceMap(ptr, sx, sy, ix, iy, glyphBufferWidth_, 2, path_, (int)wcslen(path_))) {
                TexCoord& tc = glyphRects_[GLYPH_PATH];
                tc.s0 = tc.t0 = 2;
                tc.s1 = sx-2;
                tc.t1 = sy-2;
            }

            int font_put_x = sx;
            int font_put_y = 0;
            int font_put_row = sy;
            sx = glyphBufferWidth_ - font_put_x;
            sy = glyphBufferHeight_ - font_put_y;
            ptr = glyphBuffer_ + font_put_y*glyphBufferWidth_ + font_put_x;

            if (textBlitter_.DistanceMap(ptr, sx, sy, ix, iy, glyphBufferWidth_, 2, "Loading...", 10)) {
                TexCoord& tc = glyphRects_[GLYPH_LOADING];
                tc.s0 = font_put_x + 2;
                tc.t0 = font_put_y + 2;
                tc.s1 = tc.s0 + ix;
                tc.t1 = tc.t0 + iy;
                font_put_x += sx;
                if (font_put_row<(font_put_y+sy))
                    font_put_row = font_put_y+sy;
            }
            else {
                font_put_x = 0;
                font_put_y = font_put_row;
                ptr = glyphBuffer_ + font_put_y*glyphBufferWidth_;
                sx = glyphBufferWidth_;
                sy = glyphBufferHeight_ - font_put_y;
                TexCoord& tc = glyphRects_[GLYPH_LOADING];
                if (textBlitter_.DistanceMap(ptr, sx, sy, ix, iy, glyphBufferWidth_, 2, "Loading...", 10)) {
                    tc.s0 = font_put_x + 2;
                    tc.t0 = font_put_y + 2;
                    tc.s1 = tc.s0 + ix;
                    tc.t1 = tc.t0 + iy;
                    font_put_x = sx;
                    if (font_put_row<(font_put_y+sy))
                        font_put_row = font_put_y+sy;
                }
            }

            // set initial coordinate
            TexCoord& tc = glyphRects_[GLYPH_IMAGE_NAME];
            tc.s0 = tc.s1 = 0;
            tc.t0 = tc.t1 = font_put_row;

            // glyph
            glyph_ = Texture2D::New(0, true);
            glyph_->SetAddressMode(ADDRESS_CLAMP, ADDRESS_CLAMP);
            glyph_->SetFilterMode(FILTER_BILINEAR);
            glyph_->UpdateImage((uint16)glyphBufferWidth_, (uint16) glyphBufferHeight_, FORMAT_A8, glyphBuffer_, false);
        }
        else {
            glyphBufferWidth_ = glyphBufferHeight_ = 0;
        }
    }

    return true;
}
//---------------------------------------------------------------------------------------
void Pan360::Finalize()
{
    quit_count_down_ = 0;
    if (decodeThread_.joinable()) {
        decodeThread_.join();
    }

    if (NULL!=image_pixels_) {
        free(image_pixels_);
        image_pixels_ = NULL;
    }
    image_width_ = image_height_ = image_components_ = 0;

    subtitleFxCoeff_ = subtitleFxShadowColor_ = pan360Crop_ = NULL;
    pan360Map_ = NULL;

    BL_SAFE_RELEASE(fontFx_);
    BL_SAFE_RELEASE(subtitleFx_);
    BL_SAFE_RELEASE(pan360_);
    BL_SAFE_RELEASE(glyph_);
    BL_SAFE_RELEASE(equiRect_);

    // geometry
    envelope_.Destroy();

    // text blitter
    textBlitter_.Finalize();
    if (glyphBuffer_) {
        free(glyphBuffer_);
        glyphBuffer_ = NULL;
    }
    glyphBufferWidth_ = glyphBufferHeight_ = 0;
}
//---------------------------------------------------------------------------------------
bool Pan360::FrameMove()
{
    // viewer transform
    Matrix3 pose;
    if (vrMgr_.GetHMDPose(pose)) {
        hmd_xform_ = pose;
        lost_tracking_ = 0;
    }
    else {
        ++lost_tracking_;
    }

    // quitting now?
    if (quit_count_down_>0)
        --quit_count_down_;

    if (dashboard_interrupt_)
        return (0!=quit_count_down_);

    bool update_texture_name = false;
    if (NULL!=image_pixels_) {
        std::unique_lock<std::mutex> lock(imageMutex_);
        if (NULL!=image_pixels_) {
            //bool reset_S3D = false; // true; // always reset?
            if (NULL==equiRect_) {
                equiRect_ = Texture2D::New(0);
                //reset_S3D = true;
            }

            if (NULL!=equiRect_ && 0<image_width_ && 0<image_height_) {
                if (3==image_components_) {
                    equiRect_->UpdateImage((uint16)image_width_, (uint16)image_height_, FORMAT_RGB8, image_pixels_);
                    memcpy(texture_name_, image_name_, sizeof(texture_name_));
                    update_texture_name = true;
                }
                else if (4==image_components_) { // alpha channels working?
                    equiRect_->UpdateImage((uint16)image_width_, (uint16)image_height_, FORMAT_RGBA8, image_pixels_);
                    memcpy(texture_name_, image_name_, sizeof(texture_name_));
                    update_texture_name = true;
                }
                else if (1==image_components_) { // is this working?
                    equiRect_->UpdateImage((uint16)image_width_, (uint16)image_height_, FORMAT_I8, image_pixels_);
                    memcpy(texture_name_, image_name_, sizeof(texture_name_));
                    update_texture_name = true;
                }

                //if (reset_S3D)
                {
                    if (image_height_==image_width_) {
                        s3D_ = 1;
                    }
                    else if (image_height_*2==image_width_) {
                        s3D_ = 0;
                    }
                }
            }

            free(image_pixels_);
            image_pixels_ = NULL;
            //image_width_ = image_height_ = image_components_ = 0;
        }
    }

    if (update_texture_name && glyphBuffer_ && glyph_) {
        TexCoord& tc = glyphRects_[GLYPH_IMAGE_NAME];
        int ix = tc.s0;
        int iy = tc.t0;
        int sx = glyphBufferWidth_ - ix;
        int sy = glyphBufferHeight_ - iy;
        uint8* ptr = glyphBuffer_ + iy*glyphBufferWidth_ + ix;
        if (textBlitter_.DistanceMap(ptr, sx, sy, ix, iy, glyphBufferWidth_, 2, texture_name_, (int)wcslen(texture_name_))) {
            tc.s0 += 2;
            tc.t0 += 2;
            tc.s1 = tc.s0 + ix;
            tc.t1 = tc.t0 + iy;

            // desc
            TexCoord& td = glyphRects_[GLYPH_IMAGE_DESC];
            ix = td.s0 = td.s1 = tc.s0;
            iy = td.t0 = td.t1 = tc.t1;
            sx = glyphBufferWidth_ - ix;
            sy = glyphBufferHeight_ - iy;
            ptr = glyphBuffer_ + iy*glyphBufferWidth_ + ix;
            wchar_t desc[128];
            int len = swprintf(desc, MAX_PATH, L"%dx%d  [%d/%d]", image_width_, image_height_, image_index_+1, image_totals_);
            if (textBlitter_.DistanceMap(ptr, sx, sy, ix, iy, glyphBufferWidth_, 2, desc, len)) {
                td.s0 += 2;
                td.t0 += 2;
                td.s1 = td.s0 + ix;
                td.t1 = td.t0 + iy;
            }

            // update glyph
            glyph_->UpdateImage((uint16)glyphBufferWidth_, (uint16) glyphBufferHeight_, FORMAT_A8, glyphBuffer_, false);

            // update glyph transform matrix
            image_loadtime_ = (float) mlabs::balai::system::GetTime();
            AlignGlyphTransform_();
        }
    }

    // controller response
    if (0==image_loading_) {
        VR::ControllerState cur, prev;
        for (int i=0; i<2; ++i) {
            VR::TrackedDevice const* device = vrMgr_.GetTrackedDevice(i);
            if (device && device->GetControllerState(cur, &prev)) {
                if (2==cur.OnTouchpad && 2!=prev.OnTouchpad) {
                    int img_delta_id = 0;
                    if (cur.Touchpad.x>0.75f) {
                        if (-0.5f<cur.Touchpad.y && cur.Touchpad.y<0.5f)
                            img_delta_id = 1;
                        //BL_LOG("x=%.4f y=%.4f\n", cur.Touchpad.x, cur.Touchpad.y);
                    }
                    else if (cur.Touchpad.x<-0.75f) {
                        if (-0.5f<cur.Touchpad.y && cur.Touchpad.y<0.5f)
                            img_delta_id = -1;
                        //BL_LOG("x=%.4f y=%.4f\n", cur.Touchpad.x, cur.Touchpad.y);
                    }
                    else if (cur.Touchpad.y>0.75f) {
                        if (-0.5f<cur.Touchpad.x && cur.Touchpad.x<0.5f)
                            img_delta_id = -2;
                        //BL_LOG("x=%.4f y=%.4f\n", cur.Touchpad.x, cur.Touchpad.y);
                    }
                    else if (cur.Touchpad.y<-0.75f) {
                        if (-0.5f<cur.Touchpad.x && cur.Touchpad.x<0.5f)
                            img_delta_id = 2;
                        //BL_LOG("x=%.4f y=%.4f\n", cur.Touchpad.x, cur.Touchpad.y);
                    }

                    if (0!=img_delta_id) {
                        if (-2==img_delta_id) {
                            image_index_ = 0; // the first
                        }
                        else if (2==img_delta_id) {
                            image_index_ = -1; // the last
                        }
                        else { // prev(-1) and next(+1)
                            image_index_ += img_delta_id;
                        }
                        ++session_id_;
                        image_loading_ = 1;

                        AlignGlyphTransform_();
                        break;
                    }
                }
            }
        }
    }

    return (0!=quit_count_down_);
}
//---------------------------------------------------------------------------------------
bool Pan360::Render(VR::HMD_EYE eye) const
{
    Renderer& renderer = Renderer::GetInstance();

    renderer.PushState();

    // disable Z
    renderer.SetDepthWrite(false);
    renderer.SetZTestDisable();

    renderer.SetWorldMatrix(Matrix3::Identity);

    // draw spherical video/background
    Matrix3 view(Matrix3::Identity), pose;
    float coeffs[4] = {
        // full equirectangular
        0.5f, // horizontal center texcoord
        0.15915494309189533576888376337251f, // horizontal texcoord scale:1/2pi
        0.0f, // latitude top texcoord
        0.31830988618379067153776752674503f, // vertical texcoord scale:1/pi
    };
 
    if (1==s3D_) {
        if (VR::HMD_EYE_RIGHT==eye)
            coeffs[2] = 0.5f;
        coeffs[3] = coeffs[1]; // 0.5/pi
    }
    else if (2==s3D_) {
        coeffs[0] = (VR::HMD_EYE_RIGHT==eye) ? 0.75f:0.25f;
        coeffs[1] = 0.07957747154594766788444188168626f; // 0.5/2pi
    }

    vrMgr_.GetHMDPose(pose, eye);
    pose.SetOrigin(0.0f, 0.0f, 0.0f);
    gfxBuildViewMatrixFromLTM(view, view.SetEulerAngles(0.0f, 0.0f, azimuth_adjust_)*pose);
    renderer.PushViewMatrix(view);
    renderer.SetEffect(pan360_);
    pan360_->BindConstant(pan360Crop_, coeffs);
    pan360_->BindSampler(pan360Map_, equiRect_);
    renderer.CommitChanges();
    envelope_.Draw();

    renderer.PopViewMatrix();
 
    //renderer.SetZTest();
    renderer.SetDepthWrite(true);

    // draw image name/desc
    if (0==image_loading_) {
        if (image_loadtime_+5.0f>mlabs::balai::system::GetTime() && glyph_) {
            TexCoord const t = glyphRects_[GLYPH_IMAGE_NAME];
            if (t.s0<t.s1 && t.t0<t.t1) {
                renderer.SetWorldMatrix(glyph_xform_);
                renderer.SetBlendMode(GFXBLEND_SRCALPHA, GFXBLEND_INVSRCALPHA);

                Primitives& prim = Primitives::GetInstance();
                renderer.SetEffect(fontFx_);
                fontFx_->SetSampler(shader::semantics::diffuseMap, glyph_);
                prim.BeginDraw(GFXPT_QUADLIST);

                float const s0 = float(t.s0)/glyphBufferWidth_;
                float const s1 = float(t.s1)/glyphBufferWidth_;
                float const t0 = float(t.t0)/glyphBufferHeight_;
                float const t1 = float(t.t1)/glyphBufferHeight_;
                float const dy = 0.06f;
                float const dx = dy * float(t.s1-t.s0)/float(t.t1-t.t0);
                float const z0 = 0.5f*dy;
                float const z1 = z0 - dy;
                float const yy = 0.5f;
                float const x0 = -0.5f*dx;
                float const x1 = x0 + dx;

                //prim.SetColor(Color::Purple);
                prim.AddVertex(x0, yy, z0, s0, t0);
                prim.AddVertex(x0, yy, z1, s0, t1);
                prim.AddVertex(x1, yy, z1, s1, t1);
                prim.AddVertex(x1, yy, z0, s1, t0);

                prim.EndDraw();

                //
                // shadow pass
                //

                renderer.SetBlendMode(GFXBLEND_ONE, GFXBLEND_ZERO);
            }
        }
    }
    else {
        TexCoord const t = glyphRects_[GLYPH_LOADING];
        if (t.s0<t.s1 && t.t0<t.t1) {
            renderer.SetWorldMatrix(glyph_xform_);
            renderer.SetBlendMode(GFXBLEND_SRCALPHA, GFXBLEND_INVSRCALPHA);

            Primitives& prim = Primitives::GetInstance();
            renderer.SetEffect(fontFx_);
            fontFx_->SetSampler(shader::semantics::diffuseMap, glyph_);
            prim.BeginDraw(GFXPT_QUADLIST);

            float const s0 = float(t.s0)/glyphBufferWidth_;
            float const s1 = float(t.s1)/glyphBufferWidth_;
            float const t0 = float(t.t0)/glyphBufferHeight_;
            float const t1 = float(t.t1)/glyphBufferHeight_;
            float const dy = 0.06f;
            float const dx = dy * float(t.s1-t.s0)/float(t.t1-t.t0);
            float const z0 = 0.5f*dy;
            float const z1 = z0 - dy;
            float const yy = 0.5f;
            float const x0 = -0.5f*dx;
            float const x1 = x0 + dx;

            //prim.SetColor(Color::Purple);
            prim.AddVertex(x0, yy, z0, s0, t0);
            prim.AddVertex(x0, yy, z1, s0, t1);
            prim.AddVertex(x1, yy, z1, s1, t1);
            prim.AddVertex(x1, yy, z0, s1, t0);

            prim.EndDraw();

            //
            // shadow pass
            //

            renderer.SetBlendMode(GFXBLEND_ONE, GFXBLEND_ZERO);
        }
    }

    // enable Z write
    renderer.SetZTest();
    //renderer.SetDepthWrite(true);

    for (int i=0; i<2; ++i) {
        VR::TrackedDevice const* device = vrMgr_.GetTrackedDevice(i);
        if (device)
            device->DrawSelf();
    }

    renderer.PopState();
    return (0!=quit_count_down_);
}

//---------------------------------------------------------------------------------------
float Pan360::DrawInfo(Color const& color, float x0, float y0, float size) const
{
    if (glyph_ && glyphBufferWidth_>0 && glyphBufferHeight_>0 && size>0.0f) {
        TexCoord const& t = glyphRects_[GLYPH_PATH];
        if (t.s0<t.s1 && t.t0<t.t1) {
            Renderer& renderer = Renderer::GetInstance();
            Primitives& prim = Primitives::GetInstance();

            renderer.PushState();
            renderer.SetBlendMode(GFXBLEND_SRCALPHA, GFXBLEND_INVSRCALPHA);
            renderer.SetDepthWrite(false);
            renderer.SetZTestDisable();

            renderer.SetEffect(fontFx_);
            fontFx_->SetSampler(shader::semantics::diffuseMap, glyph_);

            prim.BeginDraw(GFXPT_SCREEN_QUADLIST);

            prim.SetColor(color);
            
            float dh = 0.0f;
            float x1 = x0 + size*(t.s1-t.s0)/(t.t1-t.t0)/renderer.GetFramebufferAspectRatio();
            float y1 = y0 + size; dh += size;
            float s0 = float(t.s0)/glyphBufferWidth_;
            float s1 = float(t.s1)/glyphBufferWidth_;
            float t0 = float(t.t0)/glyphBufferHeight_;
            float t1 = float(t.t1)/glyphBufferHeight_;
            prim.AddVertex2D(x0, y0, s0, t0);
            prim.AddVertex2D(x0, y1, s0, t1);
            prim.AddVertex2D(x1, y1, s1, t1);
            prim.AddVertex2D(x1, y0, s1, t0);

            TexCoord const& t2 = glyphRects_[(0==image_loading_) ? GLYPH_IMAGE_NAME:GLYPH_LOADING];
            if (t2.s0<t2.s1 && t2.t0<t2.t1) {
                x1 = x0 + size*(t2.s1-t2.s0)/(t2.t1-t2.t0)/renderer.GetFramebufferAspectRatio();
                y0 = y1;
                y1 = y0 + size; dh += size;
                s0 = float(t2.s0)/glyphBufferWidth_;
                s1 = float(t2.s1)/glyphBufferWidth_;
                t0 = float(t2.t0)/glyphBufferHeight_;
                t1 = float(t2.t1)/glyphBufferHeight_;
                prim.AddVertex2D(x0, y0, s0, t0);
                prim.AddVertex2D(x0, y1, s0, t1);
                prim.AddVertex2D(x1, y1, s1, t1);
                prim.AddVertex2D(x1, y0, s1, t0);

                if (0==image_loading_) {
                    TexCoord const& t3 = glyphRects_[GLYPH_IMAGE_DESC];
                    if (t3.s0<t3.s1 && t3.t0<t3.t1) {
                        x1 = x0 + size*(t3.s1-t3.s0)/(t3.t1-t3.t0)/renderer.GetFramebufferAspectRatio();
                        y0 = y1;
                        y1 = y0 + size; dh += size;
                        s0 = float(t3.s0)/glyphBufferWidth_;
                        s1 = float(t3.s1)/glyphBufferWidth_;
                        t0 = float(t3.t0)/glyphBufferHeight_;
                        t1 = float(t3.t1)/glyphBufferHeight_;
                        prim.AddVertex2D(x0, y0, s0, t0);
                        prim.AddVertex2D(x0, y1, s0, t1);
                        prim.AddVertex2D(x1, y1, s1, t1);
                        prim.AddVertex2D(x1, y0, s1, t0);
                    }
                }
            }

            prim.EndDraw();

            renderer.PopState();

            return dh;
        }
    }
    return 0.0f;
}

} // namespace htc
