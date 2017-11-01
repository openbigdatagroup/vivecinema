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
 * @file    SystemFont.h
 * @author  andre chen
 * @history 2016/07/28 created
 *
 */
#ifndef SYSTEM_FONT_H
#define SYSTEM_FONT_H

#include "ISO639.h"
#include "BLCore.h"

//#include "BLPNG.h"

namespace mlabs { namespace balai { namespace system {

struct FontParam {
    enum { MAX_NAME_LENGTH = 64 };
    char name[MAX_NAME_LENGTH]; // utf-8
    int  size;
    uint8 bold; // 0, 1, 2, 3
    uint8 italic;
    uint8 underline;
    uint8 strikeout;
    FontParam():size(16),bold(0),italic(0),underline(0),strikeout(0) {
        memcpy(name, "Arial", 6);
    }
    bool operator==(FontParam const& that) const {
        if (size!=that.size || bold!=that.bold || italic!=that.italic ||
            underline!=that.underline || strikeout!=that.strikeout)
            return false;
        for (int i=0; '\0'!=name[i] && i<MAX_NAME_LENGTH; ++i) {
            if (name[i]!=that.name[i])
                return false;
        }
        return true;
    }
    bool operator!=(FontParam const& that) const {
        if (size==that.size && bold==that.bold && italic==that.italic &&
            underline==that.underline && strikeout==that.strikeout) {
            for (int i=0; i<MAX_NAME_LENGTH; ++i) {
                if (name[i]!=that.name[i])
                    return true;

                if ('\0'==name[i])
                    return false;
            }
        }
        return true;
    }
};

bool IsFontAvailable(FontParam const& font, mlabs::balai::ISO_639 lan, int codepage);

class ISystemFont
{
protected:
    enum { MIN_LENGTH = 256 };
    ISystemFont() {}
    virtual ~ISystemFont() {}

public:
    virtual bool Initialize(int font_height, char const* fontname=nullptr, bool bold=true, mlabs::balai::ISO_639 lan=mlabs::balai::ISO_639_ENG) = 0;
    virtual bool Initialize(FontParam const& param, mlabs::balai::ISO_639 lan=mlabs::balai::ISO_639_UNKNOWN) = 0;

    //
    // andre : pardon me... the code page is free to be interpreted, but we're now
    //         favoring windows definition.
    // https://msdn.microsoft.com/zh-tw/library/windows/desktop/dd317756(v=vs.85).aspx
    //         
    virtual bool Initialize(int font_height, char const* fontname=nullptr, bool bold=true, int codepage=0) = 0;
    virtual bool Initialize(FontParam const& param, int codepage=0) = 0;

    virtual bool GetMaxTextExtend(int& width, int& height) const = 0;

    // ANSI version
    virtual bool GetTextExtend(int& width, int& height, char const* text, int len) const = 0;
    virtual bool BlitText(uint8* glyph, int stride,
                          int width, int height, char const* text, int len) const = 0;
    virtual uint8 const* GetTextRect(int& width, int& height, char const* text, int len) const = 0;

    // wide character version
    virtual bool GetTextExtend(int& width, int& height, wchar_t const* text, int len) const = 0;
    virtual bool BlitText(uint8* glyph, int stride,
                          int width, int height, wchar_t const* text, int len) const = 0;
    virtual uint8 const* GetTextRect(int& width, int& height, wchar_t const* text, int len) const = 0;

    virtual void Finalize() = 0;

    // example
    //  size of glyph must be big than stride * GetMaxTextExtend().height.
    bool BlitTextSafe(uint8* glyph, int stride, int& ex, int& ey,
                      wchar_t const* text, int len) const {
        if (glyph && text && len>0) {
            int const len0 = len;
            ex = ey = 0;
            while (!GetTextExtend(ex, ey, text, len)) {
                if (--len<MIN_LENGTH)
                    return false;
            }

            wchar_t* text_clone = NULL;
            if (len<len0) {
                text_clone = (wchar_t*) malloc(len*sizeof(wchar_t));
                memcpy(text_clone, text, len*sizeof(wchar_t));
                text_clone[len-1] = L'.';
                text_clone[len-2] = L'.';
                text_clone[len-3] = L'.';
                ex = ey = 0;
                if (!GetTextExtend(ex, ey, text_clone, len)) {
                    free(text_clone);
                    return false;
                }
                text = text_clone;
            }

            bool const succeed = BlitText(glyph, stride, ex, ey, text, len);

            if (text_clone) {
                free(text_clone);
            }

            return succeed;
        }
        return false;
    }

    // factory
    static ISystemFont* New(); 
    static void Delete(ISystemFont*& font) {
        if (font) {
            delete font;
            font = nullptr;
        }
    }
};

// build distance map
class TextBlitter
{
    ISystemFont* font_;
    uint8* buffer_;
    int buffer_width_;
    int buffer_height_;
    int spread_factor_; // [4, 256]

    //
    // build 1-norm, a.k.a. taxicab norm or Manhattan norm, distance map.
    // trade precise for speed... i believe there will be so many better ways to
    // achieve this... (note it likely to take 2 mipmap level down-sizing,
    // maybe it eventually will be equally well~)
    //
    // Kindly tell me if you find one. Many thanks:-) andre.hl.chen@gmail.com
    //
    bool BuildDistanceMap_(int& w_out, int& h_out,
                           uint8 const* glyph, int width, int height) const {
        assert(NULL!=buffer_);
        assert(1<spread_factor_ && spread_factor_<=256);
        int const spread_steps = spread_factor_/2;
        w_out = width + spread_steps*2;
        h_out = height + spread_steps*2;
        if ((w_out*h_out)>(buffer_width_*buffer_height_))
            return false;

        memset(buffer_, 0, w_out*h_out);

        // 32/255 = 0.125, threshold for outliner pixels
        uint8 const threshold = 32;

        // find boundary(128/127) and the end(0/255)
        int d_off = 128/spread_steps;
        uint8 pd = uint8(127 + d_off); // positive distance
        uint8 nd = uint8(128 - d_off); // negative distance
#if 1
        //uint8 pdd = uint8(127 + 14*d_off/10); // diagonal positive distance
        //uint8 ndd = uint8(128 - 14*d_off/10); // diagonal negative distance
        bool rr, bb;
        //bool rb;
        int const last_col = width - 1;
        int const last_row = height - 1;
        for (int i=0; i<height; ++i) {
            uint8 const* c = glyph + i*width;
            uint8* d = buffer_ + (i+spread_steps)*w_out + spread_steps;
            for (int j=0; j<width; ++j,++c,++d) {
                if (c[0]>threshold) {
                    if (last_col!=j) {
                        rr = c[1]<=threshold;
                        if (last_row!=i) {
                            bb = c[width]<=threshold;
                            //rb = c[width+1]<=threshold;
                        }
                        else {
                            //rb = false;
                            bb = true;
                        }
                    }
                    else {
                        rr = true;
                        if (last_row!=i) {
                            bb = c[width]<=threshold;
                        }
                        else {
                            bb = true;
                        }
                        //rb = false;
                    }

                    if (rr) {
                        d[0] = pd;
                        d[1] = nd;
                        if (bb)
                            d[w_out] = nd;
                    }
                    else if (bb) {
                        d[0] = pd;
                        d[w_out] = nd;
                    }
                    /////////////////////////////////////
                    //else if (rb) {
                    //    d[w_out+1] = ndd;
                    //    d[0] = pdd;
                    //}
                    /////////////////////////////////////
                    else {
                        d[0] = 255;
                    }
/*
                    // could it be!?
                    if (0==i) {
                        d[-w_out] = nd;
                        d[0] = pd;
                    }

                    if (0==j) {
                        d[-1] = nd;
                        d[0] = pd;
                    }
*/
                }
                else {
                    rr = last_col!=j && c[1]>threshold;
                    bb = last_row!=i && c[width]>threshold;
                    if (rr) {
                        d[0] = nd;
                        d[1] = pd;
                        if (bb)
                            d[w_out] = pd;
                    }
                    else if (bb) {
                        d[0] = nd;
                        d[w_out] = pd;
                    }
                }
            }
        }

        uint8 last_pd = pd;
        uint8 last_nd = nd;
        int const the_last_step = spread_steps - 1; // since the last step (0, 255) are done
        for (int k=1; k<the_last_step; ++k) {
            d_off = (k+1)*128/spread_steps;
            assert(d_off<129);
            pd = uint8(127 + d_off);
            nd = uint8(128 - d_off);
            assert(last_pd<pd && pd<255);
            assert(0<nd && nd<last_nd);
            for (int i=2; i<h_out; ++i) {
                uint8* d = buffer_ + w_out*(i-1) + 1;
                for (int j=2; j<w_out; ++j,++d) {
                    if (0==d[0]) {
                        if (d[-1]==last_nd || d[1]==last_nd || d[-w_out]==last_nd || d[w_out]==last_nd)
                            d[0] = nd;
                    }
                    else if (255==d[0]) {
                        if (d[-1]==last_pd || d[1]==last_pd || d[-w_out]==last_pd || d[w_out]==last_pd)
                            d[0] = pd;
                    }
                }
            }
            last_pd = pd;
            last_nd = nd;
        }
#else
////////////////////
// to be removed! //
////////////////////
        for (int i=1; i<height; ++i) {
            uint8 const* c = glyph + i*width + 1;
            uint8* d = buffer_ + (i+spread_steps)*w_out + spread_steps + 1;
            for (int j=1; j<width; ++j,++c,++d) {
                if (c[0]>threshold) {
                    if (c[-1]<=threshold) {
                        d[0] = pd;
                        d[-1] = nd;
                        if (c[-width]<=threshold)
                            d[-w_out] = nd;
                    }
                    else if (c[-width]<=threshold) {
                        d[0] = pd;
                        d[-w_out] = nd;
                    }
                    else {
                        d[0] = 255;
                    }
                }
                else {
                    if (c[-1]>threshold) {
                        d[0] = nd;
                        d[-1] = pd;
                        if (c[-width]>threshold)
                            d[-w_out] = pd;
                    }
                    else if (c[-width]>threshold) {
                        d[0] = nd;
                        d[-w_out] = pd;
                    }
                }
            }
        }

        uint8 last_pd = pd;
        uint8 last_nd = nd;
        int const the_last_step = spread_steps - 1; // since the last step (0, 255) are done
        for (int k=1; k<the_last_step; ++k) {
            d_off = (k+1)*128/spread_steps;
            assert(d_off<129);
            pd = uint8(127 + d_off);
            nd = uint8(128 - d_off);
            assert(last_pd<pd && pd<255);
            assert(0<nd && nd<last_nd);
            for (int i=2; i<h_out; ++i) {
                uint8* d = buffer_ + w_out*(i-1) + 1;
                for (int j=2; j<w_out; ++j,++d) {
                    if (0==d[0]) {
                        if (d[-1]==last_nd || d[1]==last_nd || d[-w_out]==last_nd || d[w_out]==last_nd)
                            d[0] = nd;
                    }
                    else if (255==d[0]) {
                        if (d[-1]==last_pd || d[1]==last_pd || d[-w_out]==last_pd || d[w_out]==last_pd)
                            d[0] = pd;
                    }
                }
            }
            last_pd = pd;
            last_nd = nd;
        }
#endif
        return true;
    }
    void BoxFilterDown_(uint8* dst, int w, int h, int downscale, int stride, uint8 const* src, int w0, int h0) const {
        assert(downscale>1);
        int const w1 = w0/downscale; assert(w1==w || (w1+1)==w);
        int const h1 = h0/downscale; assert(h1==h || (h1+1)==h);
        int const ww = w1*downscale;

        int const samples = downscale*downscale;
        int const half_samples = samples/2;

        int const samples_w = (w0-ww)*downscale;
        int const half_samples_w = samples_w/2;

        for (int i=0; i<h1; ++i) {
            uint8* d = dst + i*stride;
            for (int j=0; j<w1; ++j) {
                int sumup = half_samples;
                uint8 const* corner = src + (i*downscale)*w0 + j*downscale;
                for (int ii=0; ii<downscale; ++ii) {
                    uint8 const* s = corner + ii*w0;
                    for (int jj=0; jj<downscale; ++jj) {
                        sumup += *s++;
                    }
                }
                assert((sumup/samples)<256);
                *d++ = (uint8) (sumup/samples);
            }

            if (w1<w) {
                int sumup = half_samples_w;
                uint8 const* corner = src + (i*downscale)*w0 + ww;
                for (int ii=0; ii<downscale; ++ii) {
                    uint8 const* s = corner + ii*w0;
                    for (int jj=ww; jj<w0; ++jj) {
                        sumup += *s++;
                    }
                }
                assert((sumup/samples_w)<256);
                *d++ = (uint8) (sumup/samples_w);
            }
        }

        if (h1<h) {
            int const hh = h1*downscale;
            int const samples_h = (h0-hh)*downscale;
            int const half_samples_h = samples_w/2;

            uint8* d = dst + h1*stride;
            for (int j=0; j<w1; ++j) {
                int sumup = half_samples_h;
                for (int ii=hh; ii<h0; ++ii) {
                    uint8 const* s = src + ii*w0 + j*downscale;
                    for (int jj=0; jj<downscale; ++jj) {
                        sumup += *s++;
                    }
                }
                assert((sumup/samples_h)<256);
                *d++ = (uint8) (sumup/samples_h);
            }

            if (w1<w) {
                src += hh*w0 + ww;
                int sumup = 0;
                for (int i=hh; i<h0; ++i,src+=w0) {
                    uint8 const* s = src;
                    for (int j=ww; j<w0; ++j) {
                        sumup += *s++;
                    }
                }

                int const frac_samples = (w0-ww)*(h0-hh);
                *d++ = (uint8) ((sumup + (frac_samples/2))/frac_samples);
            }
        }
    }

public:
    TextBlitter():font_(NULL),buffer_(NULL),buffer_width_(0),buffer_height_(0),spread_factor_(0) {}
    ~TextBlitter() { Finalize(); }

    // miplevel(pyramid) = 0, 1, 2, 3, 4
    template<typename TYPE>
    bool Create(char const* fontname, int font_height, bool bold, TYPE lan, int spread_factor=0) {
        Finalize();
        font_ = ISystemFont::New();
        if (NULL!=font_) {
            if (font_->Initialize(font_height, fontname, bold, lan)) {
                font_->GetMaxTextExtend(buffer_width_, buffer_height_);
                if (spread_factor>256) {
                    spread_factor_ = 256;
                }
                else if (spread_factor<4) {
                    spread_factor_ = (0==spread_factor) ? (buffer_height_/4):4;
                }
                else {
                    spread_factor_ = spread_factor;
                }
                buffer_width_ += spread_factor_;
                buffer_height_ += spread_factor_;

                buffer_ = (uint8*) malloc(buffer_width_*buffer_height_);

                return (NULL!=buffer_);
            }

            ISystemFont::Delete(font_);
        }
        return false;
    }

    template<typename TYPE>
    bool Create(FontParam const& param, TYPE lan, int spread_factor=0) {
        Finalize();
        font_ = ISystemFont::New();
        if (NULL!=font_) {
            if (font_->Initialize(param, lan)) {
                font_->GetMaxTextExtend(buffer_width_, buffer_height_);
                if (spread_factor>256) {
                    spread_factor_ = 256;
                }
                else if (spread_factor<4) {
                    spread_factor_ = (0==spread_factor) ? (buffer_height_/4):4;
                }
                else {
                    spread_factor_ = spread_factor;
                }
                buffer_width_ += spread_factor_;
                buffer_height_ += spread_factor_;

                buffer_ = (uint8*) malloc(buffer_width_*buffer_height_);

                return (NULL!=buffer_);
            }

            ISystemFont::Delete(font_);
        }
        return false;
    }

    void Finalize() {
        ISystemFont::Delete(font_);
        if (NULL!=buffer_) {
            free(buffer_);
            buffer_ = NULL;
        }
        buffer_width_ = buffer_height_ = 0;
    }

    bool IsReady() const { return NULL!=font_ && NULL!=buffer_; }

    bool GetMaxTextExtend(int& width, int& height, int pyramid) const {
        if (NULL!=font_ && font_->GetMaxTextExtend(width, height)) {
            int const downscale = (pyramid<1) ? 1:(1<<pyramid);
            width = (width+spread_factor_+downscale-1)/downscale;
            height = (height+spread_factor_+downscale-1)/downscale;
            return true;
        }
        return false;
    }

    template<typename T>
    bool GetTextExtend(int& width, int& height, int pyramid, bool distanceMap, T const* text, int len) const {
        if (NULL!=font_ && font_->GetTextExtend(width, height, text, len)) {
            int const downscale = (pyramid<1) ? 1:(1<<pyramid);
            int const roundup = distanceMap ? (spread_factor_+downscale-1):(downscale-1);
            width = (width+roundup)/downscale;
            height = (height+roundup)/downscale;
            return true;
        }
        return false;
    }

    int GetDistanceMapSafeBorder(int pyramid) const {
        if (NULL!=font_) {
            return 1 + ((spread_factor_+1/2)/((pyramid<1) ? 1:(1<<pyramid)));
        }
        return 0;
    }

    template<typename T>
    bool Bitmap(uint8* glyph, int& width, int& height, int stride, int pyramid, T const* text, int len) const {
        int w0(0), h0(0);
        if (NULL!=glyph && NULL!=font_) {
            int const downscale = (pyramid<1) ? 1:(1<<pyramid);
            if (downscale>1) {
                uint8 const* pixels = font_->GetTextRect(w0, h0, text, len);
                if (NULL!=pixels) {
                    int const w1 = (w0+downscale-1)/downscale;
                    int const h1 = (h0+downscale-1)/downscale;
                    if (stride>=w1 && width>=w1 && height>=h1) {
                        width = w1; height = h1;
                        BoxFilterDown_(glyph, width, height, downscale, stride, pixels, w0, h0);
                        return true;
                    }
                    w0 = w1; h0 = h1; // failed(see below)
                }
            }
            else {
                 if (font_->GetTextExtend(w0, h0, text, len)) {
                     if (width>=w0 && height>=h0) {
                        width = w0; height = h0;
                        return font_->BlitText(glyph, stride, width, height, text, len);
                     }
                 }
            }
        }
        width = w0; height = h0;
        return false;
    }

    template<typename T>
    bool DistanceMap(uint8* dist, int& w_out, int& h_out, int& w_in, int& h_in,
                     int stride, int pyramid, T const* text, int len) const {
        uint8 const* pixels = NULL;
        if (NULL!=dist && NULL!=font_ && NULL!=(pixels=font_->GetTextRect(w_in, h_in, text, len))) {
            int const downscale = (1<<pyramid);
            int w(0), h(0);
            if (!BuildDistanceMap_(w, h, pixels, w_in, h_in)) {
                w_out = h_out = 0; // internal buffer too small
                return false;
            }

            int const w0 = w;
            int const h0 = h;
            w = (w+downscale-1)/downscale;
            h = (h+downscale-1)/downscale;

            // can not mix column and row... to check (w_out*h_out)<(w*h) is wrong!!!
            if (w_out<w || h_out<h || stride<w)
                return false;

            if (pyramid>0) {
                w_out = w;
                h_out = h;
                w_in = (w_in+downscale-1)/downscale;
                h_in = (h_in+downscale-1)/downscale;
                BoxFilterDown_(dist, w, h, downscale, stride, buffer_, w0, h0);
/*
                static int id = 0;
                if (++id>32) {
                    char filename[256];
                    sprintf(filename, "./video/test/subtitle/frozen/dmap_%d_1.png", id-32);
                    mlabs::balai::image::write_PNG(filename, buffer_, w0, h0, 1, 8);
                    sprintf(filename, "./video/test/subtitle/frozen/dmap_%d_2.png", id-32);
                    mlabs::balai::image::write_PNG(filename, dist, stride, h, 1, 8);
                }*/
            }
            else {
                w_out = w_in = w;
                h_out = h_in = h;
                uint8 const* ptr = buffer_;
                for (int i=0; i<h_out; ++i,dist+=stride,ptr+=w_out) {
                    memcpy(dist, ptr, w_out);
                }
            }
            return true;
        }
        return false;
    }
};

}}}

#endif