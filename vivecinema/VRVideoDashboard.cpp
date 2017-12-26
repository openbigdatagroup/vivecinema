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
 * @file    VRVideoDashboard.cpp
 * @author  andre chen
 * @history 2016/03/21 created
 *
 */
#include "VRVideoPlayer.h"
#include "BLPrimitives.h"

using namespace mlabs::balai::math;
using namespace mlabs::balai::graphics;

// an unusual color in nature
static mlabs::balai::graphics::Color tweakable_color = { 255, 128, 0, 255 };

namespace htc {

inline int OpenVideoForDecodingImage(AVFormatContext*& formatCtx,
                     AVCodecContext*& videoCodecCtx,
                     SwsContext*& swsCtx,
                     int w, int h,
                     char const* mpeg) {
    formatCtx = NULL;
    videoCodecCtx = NULL;
    swsCtx = NULL;
    if (NULL!=mpeg) {
        if (avformat_open_input(&formatCtx, mpeg, NULL, NULL)!=0) {
            BL_ERR("avformat_open_input(%s) failed!\n", mpeg);
            return -1;
        }

        // retrieve stream information
        if (avformat_find_stream_info(formatCtx, NULL)<0) {
            BL_ERR("avformat_find_stream_info() failed!\n");
            avformat_close_input(&formatCtx);
            formatCtx = NULL;
            return -1;
        }

        //
        // use best video stream!?
        //int best_vid_stream = av_find_best_stream(formatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
        //

        int video_stream_id = -1;
        for (int i=0; i<(int)formatCtx->nb_streams; ++i) {
            AVStream* stream = formatCtx->streams[i];
            if (-1==video_stream_id && AVMEDIA_TYPE_VIDEO==FFmpeg_Codec_Type(stream)) {
                videoCodecCtx = FFmpeg_AVCodecOpen(stream);
                if (NULL!=videoCodecCtx) {
                    swsCtx = sws_getContext(videoCodecCtx->width, videoCodecCtx->height, videoCodecCtx->pix_fmt,
                                        w, h, AV_PIX_FMT_RGB24,
                                        SWS_BILINEAR, NULL, NULL, NULL);
                    if (NULL!=swsCtx) {
                        video_stream_id = i;
                    }
                    else {
                        avcodec_close(videoCodecCtx);
                        avcodec_free_context(&videoCodecCtx);
                    }
                }
            }
            else {
                stream->discard = AVDISCARD_ALL;
            }
        }

        if (0<=video_stream_id) {
            return video_stream_id;
        }

        avformat_close_input(&formatCtx);
        formatCtx = NULL;
        videoCodecCtx = NULL;
        swsCtx = NULL;
    }
    return -1;
}
//---------------------------------------------------------------------------------------
void VRVideoPlayer::DrawText_(Matrix3 const& xform,
                              ITexture* font, float font_size,
                              Color const& color, TexCoord const& tc) const
{
    float const z1 = 0.5f*font_size;
    float const x1 = z1*(tc.x1-tc.x0)/(tc.y1-tc.y0);
    Renderer& renderer = Renderer::GetInstance();
    Primitives& prim = Primitives::GetInstance();
    renderer.SetWorldMatrix(xform);
    renderer.PushState();
    renderer.SetCullDisable();
    renderer.SetBlendMode(GFXBLEND_SRCALPHA, GFXBLEND_INVSRCALPHA);
    renderer.SetEffect(fontFx_);
    fontFx_->SetSampler(shader::semantics::diffuseMap, font);
    prim.BeginDraw(GFXPT_QUADLIST);
        prim.SetColor(color);
        prim.AddVertex(-x1, 0.0f, z1, tc.x0, tc.y0);
        prim.AddVertex(-x1, 0.0f, -z1, tc.x0, tc.y1);
        prim.AddVertex(x1, 0.0f, -z1, tc.x1, tc.y1);
        prim.AddVertex(x1, 0.0f, z1, tc.x1, tc.y0);
    prim.EndDraw();
    renderer.PopState();
}
//---------------------------------------------------------------------------------------
void VRVideoPlayer::DrawSubtitle_(mlabs::balai::math::Matrix3 const& xform, int timestamp,
                                  Rectangle const& screen, bool clipping, bool is360) const
{
    assert(NULL!=current_track_);
    Renderer& renderer = Renderer::GetInstance();
    Primitives& prim = Primitives::GetInstance();
    renderer.SetWorldMatrix(xform);
    renderer.SetDepthWrite(false);
    renderer.SetZTestDisable();
    renderer.SetBlendMode(GFXBLEND_SRCALPHA, GFXBLEND_INVSRCALPHA);

    // project distance offset for 360 subtitle, set hfov range [42, 50]
    float const tan_half_hfov = tan(0.5f*50.0f*math::constants::float_deg_to_rad);
    float dist(1.0f), y0(0.0f), dw, dh, baseline;

    float s0(0.0f), s1(1.0f), t0(0.0f), t1(1.0f); // initial values for hardsub(FORMAT_RGBA8)
    float tt(0.0f), bb(0.0f), ll(0.0f), rr(0.0f);
    if (FORMAT_I8==subtitle_->Format()) {
        float top_offset = 0.0f;
        if (widget_on_>1) {
            float top = top_offset = subtitle_rects_[0].PlayResY;
            for (int i=0; i<subtitle_rect_count_; ++i) {
                SubtitleRect const& rt = subtitle_rects_[i];
                if (0==((1<<7)&rt.Flags) && rt.Y<top) {
                    top = rt.Y;
                }
            }
            top_offset = (top/top_offset - 0.025f)*screen.Height();
        }

        struct Paragraph {
            Vector3  v0, v1, v2, v3;
            TexCoord tc;
            Color color, outline, background;
            uint8 shadow;    // [0, 4]
            uint8 border;    // [0, 4]
            uint8 rotate;
            uint8 draw;
        } paragraphes[Subtitle::MAX_NUM_SUBTITLE_RECTS*2];
        int total_paragraphes = 0;
        int total_rotates = 0;
        int total_opaque_boxes = 0;
        for (int i=0; i<subtitle_rect_count_ && (total_paragraphes/2)<Subtitle::MAX_NUM_SUBTITLE_RECTS; ++i) {
            SubtitleRect const& rect = subtitle_rects_[i];
            if (rect.TimeStart<=timestamp && timestamp<rect.TimeEnd) {
                Paragraph& par = paragraphes[total_paragraphes];

                par.color = rect.PrimaryColour;
                int const fade_alpha = rect.FadeAlpha(timestamp);
                if (0<=fade_alpha && fade_alpha<255) {
                    par.color.a = (uint8)((fade_alpha*par.color.a)/255);
                }
                par.outline = rect.OutlineColour;
                par.background = rect.BackColour;

                // texture coordinate (may be cropped)
                s0 = rect.s0; s1 = rect.s1;
                t0 = rect.t0; t1 = rect.t1;

                // position
                rect.GetExtent(ll, tt, rr, bb, timestamp);
                if (is360) {
                    y0 = -0.01f*rect.Layer;
                    dist = 1.0f + y0;
                    if (dist<0.0f) {
                        dist = 0.01f; // could be still invisible
                        y0 = dist - 1.0f;
                    }

                    dw = dist*tan_half_hfov;
                    dh = dw*rect.PlayResY/rect.PlayResX;
                    baseline = -dw*0.95f;

                    ll = -dw + 2.0f*dw*ll/rect.PlayResX;
                    rr = ll + 2.0f*dw*rr/rect.PlayResX;
                    tt = dh - 2.0f*dh*tt/rect.PlayResY + (baseline+dh);
                    bb = tt - 2.0f*dh*bb/rect.PlayResY;
                }
                else {
                    y0 = 0.0f;
                    tt = screen.z0 - screen.Height()*tt/rect.PlayResY;
                    if (0==((1<<7)&rect.Flags)) {
                        tt += top_offset;
                    }
                    bb = tt - screen.Height()*bb/rect.PlayResY;
                    ll = screen.x0 + screen.Width()*ll/rect.PlayResX;
                    rr = ll + screen.Width()*rr/rect.PlayResX;
                }

                par.shadow = rect.Shadow;
                par.border = rect.Border;
                par.draw = 1;
                if (par.border&0x80) {
                    ++total_opaque_boxes;
                }

                float xOrg(0.0f), yOrg(0.0f), yaw(0.0f), pitch(0.0f), roll(0.0f);
                if (!rect.GetRotationParams(xOrg, yOrg, yaw, roll, pitch, timestamp)) {
                    if (clipping) {
                        if (rr<screen.x0 || ll>screen.x1 || tt<screen.z1 || bb>screen.z0)
                            continue;

                        if (ll<screen.x0) {
                            s0 += (s1-s0)*(screen.x0-ll)/(rr-ll);
                            ll = screen.x0;
                        }
                        if (rr>screen.x1) {
                            s1 = s0 + (s1-s0)*(screen.x1-ll)/(rr-ll);
                            rr = screen.x1;
                        }
                        if (tt>screen.z0) {
                            t0 += (t1-t0)*(tt-screen.z0)/(tt-bb);
                            tt = screen.z0;
                        }
                        if (bb<screen.z1) {
                            t1 = t0 + (t1-t0)*(tt-screen.z1)/(tt-bb);
                            bb = screen.z1;
                        }
                    }
 
                    par.rotate = 0;
                    par.v0 = Vector3(ll, y0, tt);
                    par.v1 = Vector3(ll, y0, bb);
                    par.v2 = Vector3(rr, y0, bb);
                    par.v3 = Vector3(rr, y0, tt);

                    TexCoord& tc = par.tc;
                    tc.x0 = s0; tc.x1 = s1;
                    tc.y0 = t0; tc.y1 = t1;
                    ++total_paragraphes;

                    if (rect.Karaoke) {
                        if (timestamp<rect.TimeKaraokeIn) {
                            par.color.r = rect.SecondaryColour.r;
                            par.color.g = rect.SecondaryColour.g;
                            par.color.b = rect.SecondaryColour.b;
                            par.color.a = (uint8)((fade_alpha*rect.SecondaryColour.a)/255);
                            if (3==rect.Karaoke) {
                                par.border = 0;
                            }
                        }
                        else if (rect.Karaoke==2 && timestamp<rect.TimeKaraokeOut) { // fill
                            float const alpha = float(timestamp-rect.TimeKaraokeIn)/float(rect.TimeKaraokeOut-rect.TimeKaraokeIn);
                            Paragraph& par2 = paragraphes[total_paragraphes++];
                            par2 = par;
                            if (par2.border&0x80) {
                                ++total_opaque_boxes;
                            }

                            par2.v0.x = par2.v1.x = par.v2.x = par.v3.x = ll + alpha*(rr-ll);
                            par2.tc.x0 = tc.x1 = s0 + alpha*(s1-s0);

                            par2.color.r = rect.SecondaryColour.r;
                            par2.color.g = rect.SecondaryColour.g;
                            par2.color.b = rect.SecondaryColour.b;
                            par2.color.a = (uint8)((fade_alpha*rect.SecondaryColour.a)/255);
                        }
                    }
                }
                else {
                    Vector3 const o(screen.x0 + screen.Width()*xOrg/rect.PlayResX,
                                    0.0f,
                                    screen.z0 - screen.Height()*yOrg/rect.PlayResY);

                    Matrix3 rot(o.x, o.y, o.z);
                    rot.SetEulerAngles(pitch, roll, yaw);
                    par.v0 = rot.PointTransform(Vector3(ll, y0, tt)-o);
                    par.v1 = rot.PointTransform(Vector3(ll, y0, bb)-o);
                    par.v2 = rot.PointTransform(Vector3(rr, y0, bb)-o);
                    par.v3 = rot.PointTransform(Vector3(rr, y0, tt)-o);
                    par.rotate = 1;

                    TexCoord& tc = par.tc;
                    tc.x0 = s0; tc.x1 = s1;
                    tc.y0 = t0; tc.y1 = t1;
                    ++total_paragraphes;
                    ++total_rotates;

                    if (rect.Karaoke) {
                        if (timestamp<rect.TimeKaraokeIn) {
                            par.color.r = rect.SecondaryColour.r;
                            par.color.g = rect.SecondaryColour.g;
                            par.color.b = rect.SecondaryColour.b;
                            par.color.a = (uint8)((fade_alpha*rect.SecondaryColour.a)/255);
                            if (3==rect.Karaoke) {
                                par.border = 0;
                            }
                        }
                        else if (rect.Karaoke==2 && timestamp<rect.TimeKaraokeOut) { // fill
                            float const alpha = float(timestamp-rect.TimeKaraokeIn)/float(rect.TimeKaraokeOut-rect.TimeKaraokeIn);
                            Paragraph& par2 = paragraphes[total_paragraphes++]; ++total_rotates;
                            par2 = par;
                            if (par2.border&0x80) {
                                ++total_opaque_boxes;
                            }

                            par2.tc.x0 = tc.x1 = s0 + alpha*(s1-s0);

                            par2.v0 = par.v3 = (1.0f-alpha)*par.v0 + alpha*par.v3;
                            par2.v1 = par.v2 = (1.0f-alpha)*par.v1 + alpha*par.v2;

                            par2.v0.x = par2.v1.x = par.v2.x = par.v3.x = ll + alpha*(rr-ll);

                            par2.color.r = rect.SecondaryColour.r;
                            par2.color.g = rect.SecondaryColour.g;
                            par2.color.b = rect.SecondaryColour.b;
                            par2.color.a = (uint8)((fade_alpha*rect.SecondaryColour.a)/255);
                        }
                    }
                }
            }
        }

        if (total_rotates>0) {
            //
            // TO-DO : Clipping!
            //
            // clipping plane!?
            //
        }

        // 1st pass - simple font, no border, no shadow.
        if (total_paragraphes>0) {
            // if opaque boxes, do it here...
            if (total_opaque_boxes) {
                prim.BeginDraw(NULL, GFXPT_QUADLIST);
                for (int i=0; i<total_paragraphes; ++i) {
                    Paragraph& par = paragraphes[i];
                    if (par.border&0x80) {
                        //
                        // The actually should render like this... (ref VLC player)
                        //
                        // if (par.border&0x7f) {
                        //   render opaque box use outline color
                        // }
                        //
                        // if (par.shadow) {
                        //   render opaque box shadow use background color
                        // }
                        //
                        // or alternately...
                        // if (par.shadow) {
                        //   render opaque box with sighly right-bottom shift, background color
                        // }
                        // if (par.border&0x7f) {
                        //   render opaque box (no shift) use outline color
                        // }
                        //
                        // andre : But IMHO, To have shadow with opaque box is silly...
                        //
                        if (par.shadow || (par.border&0x7f)) {
                            Color const& color = (par.border&0x7f) ? par.outline:par.background;
                            if (color.a) {
                                prim.SetColor(color);
                                prim.AddVertex(par.v0, par.tc.x0, par.tc.y0);
                                prim.AddVertex(par.v1, par.tc.x0, par.tc.y1);
                                prim.AddVertex(par.v2, par.tc.x1, par.tc.y1);
                                prim.AddVertex(par.v3, par.tc.x1, par.tc.y0);
                            }
                        }

                        par.shadow = par.border = 0;
                    }
                }
                prim.EndDraw();
            }

            renderer.SetEffect(fontFx_);
            fontFx_->SetSampler(shader::semantics::diffuseMap, subtitle_);
            prim.BeginDraw(GFXPT_QUADLIST);
            int totals = 0;
            for (int i=0; i<total_paragraphes; ++i) {
                Paragraph const& par = paragraphes[i];
                if (par.color.a) {
                    prim.SetColor(par.color);
                    prim.AddVertex(par.v0, par.tc.x0, par.tc.y0);
                    prim.AddVertex(par.v1, par.tc.x0, par.tc.y1);
                    prim.AddVertex(par.v2, par.tc.x1, par.tc.y1);
                    prim.AddVertex(par.v3, par.tc.x1, par.tc.y0);
                }
                if (par.border || par.shadow) {
                    if (totals<i) {
                        memcpy(paragraphes+totals, &par, sizeof(Paragraph));
                    }
                    ++totals;
                }
            }
            prim.EndDraw();
            total_paragraphes = totals;
        }

        // 2nd pass - outline and shadow
        if (total_paragraphes>0) {
            float coeff[4];//, color4f[4];
            renderer.SetEffect(subtitleFx_);
            subtitleFx_->SetSampler(shader::semantics::diffuseMap, subtitle_);

            // shadow direction : bottom-right
            float const shadow_texcoord_t_offset = (subtitle_rects_[0].t0-subtitle_rects_[0].t1)*0.055f;
            float const shadow_texcoord_s_offset = shadow_texcoord_t_offset*subtitle_->Height()/subtitle_->Width();

            Paragraph const& p0 = paragraphes[0];
            Color shadow = p0.background;

            // setup shader parameters
            if (0!=p0.shadow) {
                coeff[0] = shadow_texcoord_s_offset;
                coeff[1] = shadow_texcoord_t_offset;
            }
            else {
                coeff[0] = coeff[1] = 0.0f;
                shadow.a = 0;
            }
            coeff[2] = coeff[3] = 0.0f; // not used
            subtitleFx_->BindConstant(subtitleFxCoeff_, coeff);

            coeff[0] = constants::float_one_over_255 * shadow.r;
            coeff[1] = constants::float_one_over_255 * shadow.g;
            coeff[2] = constants::float_one_over_255 * shadow.b;
            coeff[3] = constants::float_one_over_255 * shadow.a;
            subtitleFx_->BindConstant(subtitleFxShadowColor_, coeff);

            prim.BeginDraw(GFXPT_QUADLIST);
            for (int i=0; i<total_paragraphes; ++i) {
                Paragraph& par = paragraphes[i];
                if (0==par.shadow) {
                    par.background.a = 0;
                }

                bool const b1 = shadow.a>0;
                bool const b2 = par.background.a>0;
                bool const b3 = shadow!=par.background;
                if ((b1!=b2) || (b3 && b1)) {
                    prim.EndDraw();
                    if (b1!=b2) {
                        if (b2) {
                            coeff[0] = shadow_texcoord_s_offset;
                            coeff[1] = shadow_texcoord_t_offset;
                        }
                        else {
                            coeff[0] = coeff[1] = 0.0f;
                        }
                        subtitleFx_->BindConstant(subtitleFxCoeff_, coeff);
                    }

                    if (b3) {
                        shadow = par.background;
                        coeff[0] = constants::float_one_over_255 * shadow.r;
                        coeff[1] = constants::float_one_over_255 * shadow.g;
                        coeff[2] = constants::float_one_over_255 * shadow.b;
                        coeff[3] = constants::float_one_over_255 * shadow.a;
                        subtitleFx_->BindConstant(subtitleFxShadowColor_, coeff);
                    }

                    prim.BeginDraw(GFXPT_QUADLIST);
                }

                if (0==par.border)
                    par.outline.a = 0;
                prim.SetColor(par.outline);
                prim.AddVertex(par.v0, par.tc.x0, par.tc.y0);
                prim.AddVertex(par.v1, par.tc.x0, par.tc.y1);
                prim.AddVertex(par.v2, par.tc.x1, par.tc.y1);
                prim.AddVertex(par.v3, par.tc.x1, par.tc.y0);
            }
            prim.EndDraw();
        }
    }
    else { // hardsub
        assert(subtitle_rect_count_==1);
        assert(FORMAT_RGBA8==subtitle_->Format());

        VIDEO_3D const s3D = current_track_->Stereoscopic3D();
        SubtitleRect const& rect = subtitle_rects_[0];
        rect.GetExtent(ll, tt, rr, bb, timestamp);
        tt = screen.z0 - screen.Height()*tt/rect.PlayResY;
        bb = tt - screen.Height()*bb/rect.PlayResY;
        ll = screen.x0 + screen.Width()*ll/rect.PlayResX;
        rr = ll + screen.Width()*rr/rect.PlayResX;

        prim.BeginDraw(subtitle_, GFXPT_QUADLIST);
        if (VIDEO_3D_MONO!=s3D) {
            if (0==(s3D&VIDEO_3D_TOPBOTTOM)) { // SBS
                s1 = 0.5f; // use only the left half

                ll += (ll-screen.x0);
                rr = screen.x1;
            }
            else { // TB
                float const mH = 0.5f*(screen.z0 + screen.z1);
                if (tt>mH) {
                    // use only the top part
                    dh = tt - mH;
                    t1 = dh/(tt-bb); 
                    tt = bb + dh;

                    //
                    // with out knowing how many lines of this subtitle,
                    // to do scaling is too clever... in the worst case,
                    // subtitle(characters) will not stay in the same scaling!
                    // Do NOT do this...
/*
                    float const ratio = dh/screen.Height();
                    if (ratio<0.075f) {
                        float const scale = 0.075f/ratio;
                        tt = screen.z1 + scale*(tt-screen.z1);
                        bb = screen.z1 + scale*(bb-screen.z1);

                        float c = 0.5f*(rr+ll);
                        rr = (rr-ll)*scale;
                        ll = c - 0.5f*rr;
                        rr += ll;
                    }
*/
                }
            }
        }

        if (widget_on_>1 && (0==((1<<7)&rect.Flags))) {
            float const tt_top = screen.z0 - 0.05f*screen.Height();
            if (tt<tt_top) {
                bb = tt_top - (tt-bb);
                tt = tt_top;
            }
        }

        prim.AddVertex(ll, 0.0f, tt, s0, t0);
        prim.AddVertex(ll, 0.0f, bb, s0, t1);
        prim.AddVertex(rr, 0.0f, bb, s1, t1);
        prim.AddVertex(rr, 0.0f, tt, s1, t0);
        prim.EndDraw();
/*
        prim.BeginDraw(NULL, GFXPT_LINELIST);
        prim.SetColor(Color::Red);
        prim.AddVertex(ll, 0.0f, tt);
        prim.AddVertex(rr, 0.0f, tt);
        prim.AddVertex(ll, 0.0f, bb);
        prim.AddVertex(rr, 0.0f, bb);
        prim.AddVertex(ll, 0.0f, tt);
        prim.AddVertex(ll, 0.0f, bb);
        prim.AddVertex(rr, 0.0f, tt);
        prim.AddVertex(rr, 0.0f, bb);
        prim.EndDraw();
*/
    }

    renderer.SetBlendMode(GFXBLEND_ONE, GFXBLEND_ZERO);
    renderer.SetDepthWrite(true);
    renderer.SetZTest();
}
//---------------------------------------------------------------------------------------
void VRVideoPlayer::CalcDashboardThumbnailRect_()
{
    float const dw = 0.5f*dashboard_width_;
    float const dh = 0.5f*dashboard_height_;
    float const x_spacing = 0.005f*dw;
    float const z_spacing = 0.005f*dh;
    float const width  = dashboard_width_ * 0.995f;
    float const height = dashboard_height_ * 0.995f;
    float const thumbnail_width = (width - (MENU_VIDEO_THUMBNAIL_COLUMN-1)*x_spacing)/MENU_VIDEO_THUMBNAIL_COLUMN;
    float const thumbnail_height = (height - (MENU_VIDEO_THUMBNAIL_ROW-1)*z_spacing)/MENU_VIDEO_THUMBNAIL_ROW;

    for (int i=0; i<MENU_VIDEO_THUMBNAILS; ++i) {
        Rectangle& rect = thumbnailRectangles_[i];
        int const row = i/MENU_VIDEO_THUMBNAIL_COLUMN;
        int const col = i%MENU_VIDEO_THUMBNAIL_COLUMN;
        rect.x0 = -0.5f*width + (thumbnail_width+x_spacing)*col;
        rect.x1 = rect.x0 + thumbnail_width;
        rect.z0 = 0.5f*height - (thumbnail_height+z_spacing)*row;
        rect.z1 = rect.z0 - thumbnail_height;
    }
}
//---------------------------------------------------------------------------------------
void VRVideoPlayer::ThumbnailDecodeLoop_()
{
    //
    // playlist.info file header, total size = 128
    //  1. [0, 31], 32 bytes "Vive Virtual Theatre Playlist"
    //  2. [32, 63], 32 bytes "Developed by Andre & Yumei"
    //  3. [64, 87], 24 bytes "andre.hl.chen@gmail.com"
    //  4. [88, 111], 24 bytes "yumeiohya@gmail.com"
    //  5. [112, 127], 16 bytes "version:" + null + "0.0.000"
    //
    //  version:
    //   0.0.000 initial version
    //   0.0.001 encode video duration in thumbnail header width[0,1] & height[0,1]
    //   0.0.002 video type tweaking :
    //              header[8] = (uint8) video->Type();
    //              header[9] = (uint8) video->Type_Intrinsic();
    //              header[10] = spherical longitude span (degrees/6)
    //   0.0.003 save subtitle, audio stream and timestamp
    //              header[250] = audio stream
    //              header[251] = subtitle stream
    //              header[252] = timestamp[0]
    //              header[253] = timestamp[1]
    //              header[254] = timestamp[2]
    //              header[255] = timestamp[3]
    char const* const curr_version = "0.0.003"; // subject to be changed
    char const* const mini_version = "0.0.002"; // subject to be changed
    int const playlist_header_size = 128;
    int const playlist_version_offset = 120;
    uint8 playlist_header[playlist_header_size];
    char const* const version = (char const*) playlist_header + playlist_version_offset;
    memset(playlist_header, 0, playlist_header_size);
    memcpy(playlist_header, "Vive Virtual Theatre Playlist", 29); // max 32
    memcpy(playlist_header+32, "Developed by Andre & Yumei", 26); // max 32
    memcpy(playlist_header+64, "andre.hl.chen@gmail.com", 23); // 24
    memcpy(playlist_header+88, "yumeiohya@gmail.com", 19); // 24
    memcpy(playlist_header+112, "version:", 8);
    memcpy(playlist_header+120, curr_version, 7);

    //
    // thumbnail header, total 256 bytes
    //  1. first 4 bytes for width (big endian)
    //  2. follow 4 bytes for height (big endian)
    //  3. next 4 bytes { a, b, c, d }
    //      a = video type
    //      b = intrinsic video type (by metadata parsed)
    //      c = spherical longitude span (degrees/6)
    //      d = fullpath_length range [5, 255]
    //  4. char fullpath[256-12-6]; (utf8)
    //  5. 6 bytes from tail { a:250, s:251, w:252, x:253, y:254, z:255 }
    //     a = audio stream
    //     s = subtitle stream
    //     last 4 bytes { w, x, y, z } for playing timestamp(big endian)
    //
    int const thumbnail_header_size = 256; // must >= playlist_header_size
    uint8  header[thumbnail_header_size];
    uint8* full_path_length = header + 11;
    uint8* full_path = header + 12;
    int const max_fullpath_length = thumbnail_header_size - 12 - 6;

    size_t file_size = 0;
    if (NULL==thumbnail_filecache_) {
        thumbnail_filecache_ = tmpfile(); // good for some read-only storage
    }
    else {
        fseek(thumbnail_filecache_, 0, SEEK_END);
        file_size = ftell(thumbnail_filecache_);
        rewind(thumbnail_filecache_);
    }

    int const dst_stride = VIDEO_THUMBNAIL_TEXTURE_WIDTH*3;
    int const src_stride = VIDEO_THUMBNAIL_WIDTH*3;
    int const thumbnail_texture_size = VIDEO_THUMBNAIL_TEXTURE_HEIGHT*dst_stride;
    int const thumbnail_block_size = thumbnail_header_size + thumbnail_texture_size;

    // total videos, allow some bad videos may be removed from list?
    int total_videos = (int) playList_.size();
    VideoTrack* video = NULL;

    // consistency check
    FILE* file2 = NULL;
    if (file_size>0) {
        int loaded_thumbnails = 0;
        int const preload_thumbnails = int(file_size-playlist_header_size)/thumbnail_block_size;
        bool file_error = (preload_thumbnails<1) ||
                          (playlist_header_size+thumbnail_block_size*preload_thumbnails)!=(int)file_size ||
                           playlist_header_size!=fread(header, 1, playlist_header_size, thumbnail_filecache_) ||
                           0!=memcmp(header, playlist_header, playlist_version_offset);
        bool file_reorg = false;
        if (!file_error) {
            if (0!=memcmp(header+playlist_version_offset, version, 7)) {
                // version mismatch, what should we do?
                char const* file_version = (char const*) header + playlist_version_offset;
                BL_LOG("playlist file version mismatch %s(expected:%s)\n", file_version, version);
                int const match = memcmp(file_version, mini_version, 7);
                if (match<0) {
                     file_error = true;
                }
            }

            int thumbnail_block_pos = 0;
            for (int i=0; i<total_videos && !file_error && thumbnail_block_pos<preload_thumbnails; ++i) {
                video = playList_[i];
                assert(0==video->ThumbnailCache());

                // search position
                fseek(thumbnail_filecache_,
                      playlist_header_size+thumbnail_block_pos*thumbnail_block_size, SEEK_SET);

                for (int j=thumbnail_block_pos; j<preload_thumbnails; ++j) {
                    if (thumbnail_header_size==fread(header, 1, thumbnail_header_size, thumbnail_filecache_)) {
                        char const* filename = video->GetFullPath();
                        int filename_bytes = video->GetFullPathBytes();
                        if (filename_bytes>max_fullpath_length) {
                            filename += (filename_bytes - max_fullpath_length);
                            filename_bytes = max_fullpath_length;
                        }

                        if (*full_path_length==filename_bytes && 0==memcmp(full_path, filename, filename_bytes)) {
                            assert((playlist_header_size+j*thumbnail_block_size+thumbnail_header_size)==ftell(thumbnail_filecache_));
                            video->ThumbnailCache() = playlist_header_size + j*thumbnail_block_size;
                            thumbnail_block_pos = j+1; // move search position
                            ++loaded_thumbnails;
                            break;
                        }
                        file_reorg = true;
                        fseek(thumbnail_filecache_, thumbnail_texture_size, SEEK_CUR);
                    }
                    else {
                        file_error = true;
                        break;
                    }
                }
            }
        }

        if (file_error || loaded_thumbnails<1) {
            file_reorg = file_error = true;
            for (int i=0; i<total_videos; ++i) {
                playList_[i]->ThumbnailCache() = 0;
            }
        }

        if (!file_reorg && loaded_thumbnails<total_videos) {
            BL_LOG("load %d thumbnails expect %d\n", loaded_thumbnails, total_videos);
            file_reorg = true; // file open with "r+", we need to reopen it with "w+"
                               // since we'll need to write more.
        }

        if (file_reorg) {
            if (!file_error) {
                file2 = tmpfile();
                // must write file header since 0 is invalid cache value
                fwrite(playlist_header, 1, playlist_header_size, file2);
                int offset = playlist_header_size;
                for (int i=0; i<total_videos; ++i) {
                    video = playList_[i];
                    int pos = video->ThumbnailCache();
                    if (pos>0) {
                        fseek(thumbnail_filecache_, pos, SEEK_SET);
                        fread(thumbnailBuffer_, 1, thumbnail_block_size, thumbnail_filecache_);
                        fwrite(thumbnailBuffer_, 1, thumbnail_block_size, file2);
                        video->ThumbnailCache() = offset;
                        offset += thumbnail_block_size;
                    }
                }
            }

            // create a new file, reopen with "w+" (it will discard previous one)
            fclose(thumbnail_filecache_);

            // open a new file
            wchar_t wfilename[256];
            swprintf(wfilename, 256, L"%s/playlist.info", video_path_);

            //char fullpath[512];
            //WideCharToMultiByte(CP_ACP, 0, wfilename, -1, fullpath, 512, NULL, NULL);
            //thumbnail_filecache_ = fopen(fullpath, "w+b");
            thumbnail_filecache_ = _wfopen(wfilename, L"w+b");
            assert(NULL!=thumbnail_filecache_);
            if (NULL==thumbnail_filecache_) {
                fclose(file2); file2 = NULL;
                thumbnail_filecache_ = tmpfile(); // good for some read-only storage
                for (int i=0; i<total_videos; ++i) {
                    playList_[i]->ThumbnailCache() = 0;
                }
            }

            // write file header
            fwrite(playlist_header, 1, playlist_header_size, thumbnail_filecache_);
        }
    }
    else {
        // write file header - to hit here, it's a empty file.
        fwrite(playlist_header, 1, playlist_header_size, thumbnail_filecache_);
    }

    // initial publish
    AVPacket packet; av_init_packet(&packet);
    AVFrame frame; memset(&frame, 0, sizeof(AVFrame)); av_frame_unref(&frame);
    publishingThumbnailId_ = total_videos;
    int publish_id = 0;
    for (int i=0; i<total_videos; ++i) {
        while (0<=menu_page_ && publish_id==publishingThumbnailId_) {
            std::unique_lock<std::mutex> lock(thumbnailMutex_);
            thumbnailCV_.wait(lock);
        }
        if (menu_page_<0)
            break;

        video = playList_[i];

        int offset = video->ThumbnailCache();
        if (offset>0) {
            if (NULL!=file2) {
                fseek(file2, offset, SEEK_SET);
                fread(header, 1, thumbnail_header_size, file2);
                fread(thumbnailBuffer_, 1, thumbnail_texture_size, file2);

                video->ThumbnailCache() = (uint32) ftell(thumbnail_filecache_);
                fwrite(header, 1, thumbnail_header_size, thumbnail_filecache_);
                fwrite(thumbnailBuffer_, 1, thumbnail_texture_size, thumbnail_filecache_);
            }
            else {
                fseek(thumbnail_filecache_, offset, SEEK_SET);
                fread(header, 1, thumbnail_header_size, thumbnail_filecache_);
                fread(thumbnailBuffer_, 1, thumbnail_texture_size, thumbnail_filecache_);
            }
            assert(video->ThumbnailCache()==(128+(1024*768*3+256)*i));
            assert(ftell(thumbnail_filecache_)==(128+(1024*768*3+256)*(i+1)));

            // video type
            //assert(video->Type_Intrinsic()==header[9]); // not always truth!
            if (VIDEO_SOURCE_UNKNOWN==video->Source()) {
                video->TweakVideoType((VIDEO_TYPE)header[8], header[10]);
            }
            video->SetSize((header[2]<<8)|header[3], (header[6]<<8)|header[7]);

            // duration in milliseconds
            if (0==(header[0]&0x80)) {
                video->SetDuration((int(header[0])<<24)|(int(header[1])<<16)|(int(header[4])<<8)|int(header[5]));
            }

            // timestamp in millisecond
            video->SetTimestamp((int(header[252])<<24)|(int(header[253])<<16)|(int(header[254])<<8)|int(header[255]));

            //assert(0xff!=header[250]); // can't be!
            video->Audio() = (0xff!=header[250]) ? header[250]:0;
            video->Subtitle() = header[251];
        }
        else {
            // clear texture image
            memset(thumbnailBuffer_, 0, thumbnail_texture_size);

            // slow ffmpeg read
            char const* mpeg = video->GetFullPath();
            AVFormatContext* formatCtx = NULL;
            AVCodecContext*  videoCodec = NULL;
            SwsContext* swsCtx = NULL;
            int const thumbnail_width = VIDEO_THUMBNAIL_WIDTH;
            int const thumbnail_height = VIDEO_THUMBNAIL_HEIGHT;
            int thumbnails_decode = 0;
            int video_width = 0;
            int video_height = 0;
            int const streamIndex = OpenVideoForDecodingImage(formatCtx, videoCodec, swsCtx,
                                                              thumbnail_width, thumbnail_height,
                                                              mpeg);
            if (0<=streamIndex) {
                AVStream* stream = formatCtx->streams[streamIndex];
                for (int j=0; j<stream->nb_side_data; ++j) {
                    AVPacketSideData const& sd = stream->side_data[j];
                    if (AV_PKT_DATA_STEREO3D==sd.type) {
                        if (sd.size>=sizeof(AVStereo3D)) {
                            AVStereo3D* s3D = (AVStereo3D*) sd.data;
                            switch (s3D->type)
                            {
                            case AV_STEREO3D_2D:
                                video->TweakVideo3D(VIDEO_3D_MONO);
                                break;
                            case AV_STEREO3D_SIDEBYSIDE:
                                if (s3D->flags & AV_STEREO3D_FLAG_INVERT) {
                                    video->TweakVideo3D(VIDEO_3D_RIGHTLEFT);
                                }
                                else {
                                    video->TweakVideo3D(VIDEO_3D_LEFTRIGHT);
                                }
                                break;
                            case AV_STEREO3D_TOPBOTTOM:
                                if (s3D->flags & AV_STEREO3D_FLAG_INVERT) {
                                    video->TweakVideo3D(VIDEO_3D_BOTTOMTOP);
                                }
                                else {
                                    video->TweakVideo3D(VIDEO_3D_TOPBOTTOM);
                                }
                                break;

                            //
                            // TO-DO : support these...?!
                            //
                            case AV_STEREO3D_FRAMESEQUENCE:
                                //BL_LOG("Stereo3D = \"frame alternate\". To Be Implemented!?\n");
                                break;
                            case AV_STEREO3D_CHECKERBOARD:
                                //BL_LOG("Stereo3D = \"checkerboard\". To Be Implemented!?\n");
                                break;
                            case AV_STEREO3D_LINES:
                                //BL_LOG("Stereo3D = \"interleaved lines\". To Be Implemented!?\n");
                                break;
                            case AV_STEREO3D_COLUMNS:
                                //BL_LOG("Stereo3D = \"interleaved columns\". To Be Implemented!?\n");
                                break;
                            case AV_STEREO3D_SIDEBYSIDE_QUINCUNX:
                                //BL_LOG("Stereo3D = \"side by side (quincunx subsampling)\". To Be Implemented!?\n");
                                break;
                            default:
                                //BL_LOG("Stereo3D = \"unknown\"!?\n");
                                break;
                            }
                        }
                        break;
                    }
                }

                video_width  = videoCodec->width;
                video_height = videoCodec->height;

                int video_duration = 25000; // 25 secs
                int seek_pts = 15000; // bypass the first 15 secs
                bool const duration_valid = formatCtx->duration>0;
                if (duration_valid) {
                    video_duration = (int) (formatCtx->duration/1000);
                    video->SetDuration(video_duration);
                    if (seek_pts*4>video_duration)
                        seek_pts = video_duration/5;
                    else if (seek_pts*8<video_duration)
                        seek_pts = video_duration/6;
                }

                AVRational const timebase_q = { 1, AV_TIME_BASE };
                AVRational const stream_time_base = av_codec_get_pkt_timebase(videoCodec);

                int max_seek_pts = video_duration*95/100;
                int const seek_jump = (max_seek_pts-seek_pts)/5;
                int max_valid_seek_pts = 0;
                int max_valid_frame_pts = 0;
                int seek_fails = 0;
                int histogram[256];
                int giveup = 0;
                while (thumbnails_decode<4 && giveup<3) {
                    // seek
                    while (seek_fails<8) {
                        if (FFmpegSeekWrapper(formatCtx, seek_pts, streamIndex)) {
                            avcodec_flush_buffers(videoCodec);
                            if (max_valid_seek_pts<seek_pts)
                                max_valid_seek_pts = seek_pts;
                            break;
                        }
                        else {
                            if (max_valid_seek_pts>0) {
                                max_seek_pts = max_valid_seek_pts;
                            }

                            // move target pts and seek again
                            seek_pts = seek_pts*70/100;
                            if (seek_pts<0)
                                seek_pts = 0;
                            ++seek_fails;
                        }
                    }

                    // read frames
                    int attempts = 0;
                    int const th = thumbnails_decode;
                    while (th==thumbnails_decode && giveup<3) {
                        int const error = av_read_frame(formatCtx, &packet);
                        if (0!=error) {
                            if (AVERROR_EOF==error) {
                                // NOTE : this doesn't mean the video is end early!!!
                                // may still have audio data...
                                seek_pts = seek_pts*80/100;
                                ++giveup;
                            }
                            else {
                                giveup = 1000000;
                            }
                            break;
                        }
                        //assert(packet.stream_index==streamIndex);
                        if (packet.stream_index==streamIndex &&
                            0==avcodec_send_packet(videoCodec, &packet) &&
                            0==avcodec_receive_frame(videoCodec, &frame)) {
                            int64_t pts = av_frame_get_best_effort_timestamp(&frame);
                            if (AV_NOPTS_VALUE!=pts) {
                                seek_pts = (int) (av_rescale_q(pts, stream_time_base, timebase_q)/1000);
                            }
                            else if (AV_NOPTS_VALUE!=packet.pts) {
                                seek_pts = (int) (av_rescale_q(packet.pts, stream_time_base, timebase_q)/1000);
                            }
                            else if (AV_NOPTS_VALUE!=frame.pkt_dts) {
                                seek_pts = (int) (av_rescale_q(frame.pkt_dts, stream_time_base, timebase_q)/1000);
                            }

                            if (max_valid_frame_pts<seek_pts)
                                max_valid_frame_pts = seek_pts;

                            bool accpet = (++attempts>=3);
                            if (!accpet) {
                                memset(histogram, 0, sizeof(histogram));
                                int const ww = frame.width*6/10;
                                int const hh = frame.height*6/10;
                                uint8_t const* yy = frame.data[0] + ((frame.height-hh)/2)*frame.linesize[0] + (frame.width-ww)/2;
                                for (int y=0; y<hh; ++y) {
                                    uint8_t const* d = yy;
                                    for (int x=0; x<ww; ++x,++d) {
                                        ++histogram[*d];
                                    }
                                    yy += frame.linesize[0];
                                }

                                int lo(255),hi(0),totals(0);
                                for (int k=0; k<256; ++k) {
                                    if (histogram[k]>0) {
                                        totals += histogram[k];
                                        if (lo>k) lo = k;
                                        if (hi<k) hi = k;
                                    }
                                }
                                assert((ww*hh)==totals);

                                int const span = hi - lo;
                                if (span>128) {
                                    // for N(0, 1) 68% of values are within 1 standard deviation of the mean,
                                    // 99.7% of values are within 3 standard deviations of the mean
                                    int const tail = totals*16/100;
                                    int subtotals = 0;
                                    while ((subtotals+=histogram[hi])<tail) {
                                        --hi;
                                    }

                                    subtotals = 0;
                                    while ((subtotals+=histogram[lo])<tail) {
                                        ++lo;
                                    }

                                    if ((100*(hi-lo))>(20*span)) {
                                        accpet = true;
                                    }
                                }
                            }

                            if (accpet) {
                                uint8_t* dst[1] = { thumbnailBuffer_ + dst_stride*((thumbnails_decode/2)*thumbnail_height) + (thumbnails_decode%2)*src_stride };
                                int stride[1] = { dst_stride };
                                sws_scale(swsCtx, (uint8_t const* const*)frame.data, frame.linesize, 0, videoCodec->height, dst, stride);

                                // make 3 copies
                                if (1==++thumbnails_decode) {
                                    uint8_t* th11 = thumbnailBuffer_;
                                    uint8_t* th12 = th11 + src_stride;
                                    uint8_t* th21 = th11 + thumbnail_height*dst_stride;
                                    uint8_t* th22 = th21 + src_stride;
                                    for (int k=0; k<thumbnail_height; ++k) {
                                        memcpy(th12, th11, src_stride); th12+=dst_stride;
                                        memcpy(th21, th11, src_stride); th21+=dst_stride;
                                        memcpy(th22, th11, src_stride); th22+=dst_stride;
                                        th11+=dst_stride;
                                    }
                                }
#if 0
                                SwsContext* swsCtx2 = sws_getContext(videoCodec->width, videoCodec->height, videoCodec->pix_fmt,
                                                        videoCodec->width, videoCodec->height, AV_PIX_FMT_RGB24,
                                                        SWS_BILINEAR, NULL, NULL, NULL);
                                uint8* pixel = (uint8*) malloc(videoCodec->width*videoCodec->height*3);
                                dst[0] = pixel;
                                stride[0] = videoCodec->width*3;
                                sws_scale(swsCtx2, (uint8_t const* const*)frame->data, frame->linesize, 0, videoCodec->height, dst, stride);
                                sws_freeContext(swsCtx2);

                                static int id = 0;
                                char filename[128];
                                sprintf(filename, "thumbnails_%d.jpg", ++id);
                                mlabs::balai::image::write_JPEG(filename, pixel, videoCodec->width, videoCodec->height);
                                free(pixel);
#endif
                                seek_pts = (seek_pts+seek_jump)%max_seek_pts;
                                seek_fails = 0;
                            }
                            av_frame_unref(&frame);
                        }
                        av_packet_unref(&packet);
                    }
                }

                sws_freeContext(swsCtx);
                avcodec_close(videoCodec);
                avcodec_free_context(&videoCodec);
                avformat_close_input(&formatCtx);
            }

//            if (thumbnails_decode>0) {
                memset(header, 0, 256);
                header[2] = (uint8) ((video_width>>8) & 0xff);
                header[3] = (uint8) (video_width & 0xff);

                header[6] = (uint8) ((video_height>>8) & 0xff);
                header[7] = (uint8) (video_height & 0xff);

                int const fullpath_bytes = video->GetFullPathBytes();
                if (fullpath_bytes>max_fullpath_length) {
                    header[11] = (uint8) max_fullpath_length;
                    memcpy(full_path, mpeg+fullpath_bytes-max_fullpath_length, max_fullpath_length);
                }
                else {
                    memcpy(full_path, mpeg, fullpath_bytes);
                    header[11] = (uint8) fullpath_bytes;
                }
                video->ThumbnailCache() = (int) ftell(thumbnail_filecache_);

                int const duration = video->GetDuration();
                if (duration>0) {
                    header[0] = (uint8) ((duration & 0xff000000)>>24);
                    header[1] = (uint8) ((duration & 0x00ff0000)>>16);
                    header[4] = (uint8) ((duration & 0x0000ff00)>>8);
                    header[5] = (uint8) (duration & 0xff);
                }
                else {
                    header[0] = header[1] = header[4] = header[5] = 0xff;
                }

                header[8] = (uint8) video->Type();
                header[9] = (uint8) video->Type_Intrinsic();
                header[10] = 0;

                assert(video->ThumbnailCache()==(128+thumbnail_block_size*i));
                fwrite(header, 1, thumbnail_header_size, thumbnail_filecache_);
                assert(ftell(thumbnail_filecache_)==(128+thumbnail_block_size*i+256));
                fwrite(thumbnailBuffer_, 1, thumbnail_texture_size, thumbnail_filecache_);
                assert(ftell(thumbnail_filecache_)==(video->ThumbnailCache()+thumbnail_block_size));

                video->SetSize(video_width, video_height);
//            }
        }
        publishingThumbnailId_ = publish_id = i;
        //BL_LOG(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> Publishing video#%d\n", publish_id);
    }
    av_frame_unref(&frame);

    if (NULL!=file2) { // close temp file
        fclose(file2);
        file2 = NULL;
    }

    // stall for the last publish, since we're going to change publishThumbnailId_
    while (0<=menu_page_ && publish_id==publishingThumbnailId_) {
        std::unique_lock<std::mutex> lock(thumbnailMutex_);
        thumbnailCV_.wait(lock);
    }

    // let VRVideoPlayer::FrameMove() know it will be no publishing
    publishingThumbnailId_ = total_videos;

    // leave if we got all!
    if (total_videos<=MENU_VIDEO_THUMBNAIL_TEXTURES) {
        return;
    }

    // can failed videos be removed from playlist!?
    publish_id = 0; // must be here!!!!
    while (0<=menu_page_) { // stop sign
        // expensive lock... may take 12 frames time > 120 ms!
        std::unique_lock<std::mutex> lock(thumbnailMutex_);
        total_videos = (int) playList_.size(); // some videos has been moved!?
        int const total_pages = (total_videos + MENU_VIDEO_THUMBNAILS - 1)/MENU_VIDEO_THUMBNAILS;
        int const page_offsets[2] = { 1, total_pages-1 };
        int const current_page = menu_page_;
        bool const move_forward = menu_page_scroll_>0.0f;
        int const delta[3] = { 0, page_offsets[move_forward], page_offsets[!move_forward] };
        int total_publishes = 0;

//#define LOG_THUMBNAIL_PUBLISH
#ifdef LOG_THUMBNAIL_PUBLISH
        int tns[MENU_VIDEO_THUMBNAIL_TEXTURES];
        int prev_reference_tracks_[MENU_VIDEO_THUMBNAIL_TEXTURES];
        memcpy(prev_reference_tracks_, reference_tracks_, sizeof(prev_reference_tracks_));
        float const time_begin = (float) mlabs::balai::system::GetTime();
#endif
        for (int i=0; 0<=menu_page_ && i<3; ++i) {
            int const page = (menu_page_ + delta[i])%total_pages;
            for (int j=0; 0<=menu_page_ && j<MENU_VIDEO_THUMBNAILS; ++j) {
                int const id = page*MENU_VIDEO_THUMBNAILS + j;
                if (id<total_videos) {
                    video = playList_[id];
                    int const offset = video->ThumbnailCache();
                    assert(offset==(128+(1024*768*3+256)*id));
                    if (-1==video->ThumbnailTextureId() && offset>0) {
                        fseek(thumbnail_filecache_, offset+thumbnail_header_size, SEEK_SET);
                        while (0<=menu_page_ && publish_id==publishingThumbnailId_) {
                            thumbnailCV_.wait(lock);
                        }

                        fread(thumbnailBuffer_, 1, thumbnail_texture_size, thumbnail_filecache_);
                        publishingThumbnailId_ = publish_id = id;
#ifdef LOG_THUMBNAIL_PUBLISH
                        tns[total_publishes] = id;
#endif
                        ++total_publishes;
                    }
                }
                else {
                    break;
                }
            }
        }

        if (0==total_publishes) {
            while (current_page==menu_page_) {
                pageCV_.wait(lock);
            }
        }
        else {
            // wait for the last publish
            while (0<=menu_page_ && publish_id==publishingThumbnailId_) {
                thumbnailCV_.wait(lock);
            }

#ifdef LOG_THUMBNAIL_PUBLISH
            BL_LOG("current page %d : publish %d thumbnails   ", current_page, total_publishes);
            for (int i=0; i<total_publishes; ++i) {
                BL_LOG("  %d", tns[i]);
            }
            BL_LOG(" (%.1fms)\n", 1000.0f*(mlabs::balai::system::GetTime()-time_begin));
#endif
        }
    }

    //BL_LOG(">>>>>>>>>>>>>>>>>>>>>>> ThumbnailDecodeLoop_() leaving...\n");
}
//---------------------------------------------------------------------------------------
bool VRVideoPlayer::OnSelectedThumbnailTransform_(mlabs::balai::math::Matrix3& xform,
                                                  int slot, float elapsed_time) const
{
    xform = dashboard_xform_;
    if (0<=slot && slot<MENU_VIDEO_THUMBNAILS) {
        float const max_distance = dashboard_distance_*0.33f;//0.25f;
        float const dist = max_distance * (elapsed_time<animation_duration_ ? (elapsed_time/animation_duration_):1.0f);
        Rectangle const& rect = thumbnailRectangles_[slot];
        Vector3 v(0.5f*(rect.x0+rect.x1), 0.0f, 0.5f*(rect.z0+rect.z1));
        v = text_xform_.Origin() - dashboard_xform_.PointTransform(v);
        xform.SetOrigin(dashboard_xform_.Origin() + dist*v.Normalize());
        return true;
    }
    return false;
}
//---------------------------------------------------------------------------------------
bool VRVideoPlayer::GetUIWidgetRect_(Rectangle& rect, DRAW_GLYPH g) const
{
    if (DRAW_UI_MENU<=g && g<DRAW_GLYPH_TOTALS) {
        bool const show_next = playList_.size()>1;
        bool const seek_bar_enable = decoder_.GetDuration()>0;
        bool const light_enable = (NULL!=current_track_ && !current_track_->IsSpherical());
        bool const language_enable = (total_audio_streams_>1) && decoder_.AudioStreamChangeable();
        bool const subtitle_enable = (total_subtitle_streams_>0);

        float const seek_bar_top = widget_top_ - 0.1f*widget_height_;
        float const seek_bar_bottom = seek_bar_top - 0.11f*widget_height_;
        float const UI_widget_top = seek_bar_bottom;
        float const UI_widget_bottom =  widget_top_ - widget_height_;
        float const UI_widget_height_ratio = 0.5f;
        float const UI_widget_height = UI_widget_height_ratio*(UI_widget_top-UI_widget_bottom);
        float const y0 = 0.5f*(UI_widget_top+UI_widget_bottom) + 0.5f*UI_widget_height;
        float const y1 = y0 - UI_widget_height;

        switch (g)
        {
        case DRAW_UI_REPLAY: {
                TexCoord const& tc = texcoords_[DRAW_UI_REPLAY];
                rect.x0 = widget_left_ + 0.25f*UI_widget_height;
                rect.x1 = rect.x0 + UI_widget_height*tc.AspectRatio();
                rect.z0 = y0;
                rect.z1 = y1;
            } break;

        case DRAW_UI_PAUSE:
        case DRAW_UI_PLAY: {
                TexCoord const& tc = texcoords_[DRAW_UI_PAUSE];
                rect.x0 = widget_left_ + 1.75f*UI_widget_height;
                rect.x1 = rect.x0 + UI_widget_height*tc.AspectRatio();
                rect.z0 = y0;
                rect.z1 = y1;
            } break;

        case DRAW_UI_NEXT:
            if (show_next) {
                TexCoord const& tc = texcoords_[DRAW_UI_NEXT];
                rect.x0 = widget_left_ + 3.15f*UI_widget_height;
                rect.x1 = rect.x0 + UI_widget_height*tc.AspectRatio();
                rect.z0 = y0;
                rect.z1 = y1;
                break;
            }
            else {
                return false;
            }

        case DRAW_UI_VOLUME: {
                TexCoord const& tc = texcoords_[DRAW_UI_VOLUME];
                float const h = 0.7f*(UI_widget_top-UI_widget_bottom);
                rect.z0 = 0.5f*(UI_widget_top+UI_widget_bottom) + 0.5f*h;
                rect.z1 = rect.z0 - h;
                rect.x0 = widget_left_ + (show_next ? 5.15f:3.0f)*UI_widget_height;
                rect.x1 = rect.x0 + h*tc.AspectRatio();
            } break;

        case DRAW_UI_VOLUME_BAR: {
                rect.x0 = widget_left_ + (show_next ? 6.7f:4.55f)*UI_widget_height;
                rect.x1 = rect.x0 + UI_widget_height*2.5f;
                rect.z0 = y0;
                rect.z1 = y1;
            } break;

        case DRAW_UI_SUBTITLE: 
            if (subtitle_enable) {
                TexCoord const& tc = texcoords_[DRAW_UI_SUBTITLE];
                float const h = 0.8f*(UI_widget_top-UI_widget_bottom);
                rect.z0 = 0.5f*(UI_widget_top+UI_widget_bottom) + 0.5f*h;
                rect.z1 = rect.z0 - h;
                float offset = 2.1f;
                if (language_enable) {
                    offset += 1.65f;
                    if (light_enable)
                        offset += 1.45f;
                }
                else if (light_enable) {
                    offset += 1.35f;
                }
                rect.x1 = widget_left_ + widget_width_ - offset*UI_widget_height;
                rect.x0 = rect.x1 - h*tc.AspectRatio();
            }
            else {
                return false;
            }
            break;

        case DRAW_UI_LANGUAGE:
            if (language_enable) {
                TexCoord const& tc = texcoords_[DRAW_UI_LANGUAGE];
                float const h = 0.8f*(UI_widget_top-UI_widget_bottom);
                rect.z0 = 0.5f*(UI_widget_top+UI_widget_bottom) + 0.5f*h;
                rect.z1 = rect.z0 - h;
                rect.x1 = widget_left_ + widget_width_ - (light_enable ? 3.4f:2.0f)*UI_widget_height;
                rect.x0 = rect.x1 - h*tc.AspectRatio();
            }
            else {
                return false;
            }
            break;

        case DRAW_UI_LIGHT: 
            if (light_enable) {
                TexCoord const& tc = texcoords_[DRAW_UI_LIGHT];
                float const h = 0.66f*(UI_widget_top-UI_widget_bottom);
                rect.z0 = 0.5f*(UI_widget_top+UI_widget_bottom) + 0.5f*h;
                rect.z1 = rect.z0 - h;
                rect.x1 = widget_left_ + widget_width_ - 2.0f*UI_widget_height;
                rect.x0 = rect.x1 - h*tc.AspectRatio();
            }
            else {
                return false;
            }
            break;

        case DRAW_UI_MENU: {
                TexCoord const& tc = texcoords_[DRAW_UI_MENU];
                float const h = 0.85f*(UI_widget_top-UI_widget_bottom);
                rect.z0 = 0.5f*(UI_widget_top+UI_widget_bottom) + 0.5f*h;
                rect.z1 = rect.z0 - h;
                rect.x1 = widget_left_ + widget_width_ - 0.25f*UI_widget_height;
                rect.x0 = rect.x1 - h*tc.AspectRatio();
            } break;

        case DRAW_UI_SEEK_BAR: 
            if (seek_bar_enable) {
                rect.x0 = widget_left_;
                rect.x1 = widget_left_ + widget_width_;
                rect.z0 = seek_bar_top;
                rect.z1 = seek_bar_bottom;
            }
            else {
                return false;
            }
            break;

        case DRAW_UI_SETTINGS: {
                float const h = 0.13f*thumbnailRectangles_[0].Height();
                float const w = h * texcoords_[DRAW_UI_SETTINGS].AspectRatio();
                rect.x1 = thumbnailRectangles_[0].Width() - 0.33f*h;
                rect.x0 = rect.x1 - w;
                rect.z0 = -0.25f*h;
                rect.z1 = rect.z0 - h;
            } break;

        case DRAW_UI_CONFIG_RECT: {
                float const w0 = thumbnailRectangles_[0].Width();
                float const h0 = thumbnailRectangles_[0].Height();
                rect.x0 = 0.04f*w0;
                rect.x1 = w0 - rect.x0;
                rect.z0 = -0.2f*h0;
                rect.z1 = -0.85f*h0;
            } break;

        case DRAW_UI_SWITCH_3D: 
        case DRAW_UI_SWITCH_VR: {
                float const h0 = thumbnailRectangles_[0].Height();
                float const h = 0.12f*h0;
                float const w = h * texcoords_[DRAW_UI_SWITCH_BASE].AspectRatio();
                rect.x0 = 0.215f*thumbnailRectangles_[0].Width();
                rect.x1 = rect.x0 + w;
                if (DRAW_UI_SWITCH_3D==g)
                    rect.z0 = -0.3333f * h0 + 0.25f*h;
                else
                    rect.z0 = -0.6f * h0 + 0.5f*h;
                rect.z1 = rect.z0 - h;
            } break;

        case DRAW_UI_SWITCH_3D_SBS:
        case DRAW_UI_SWITCH_3D_TB: {
                float const w0 = thumbnailRectangles_[0].Width();
                float const h0 = thumbnailRectangles_[0].Height();
                float const w = 0.24f*w0;
                float const h = 0.12f*h0;

                if (DRAW_UI_SWITCH_3D_SBS==g)
                    rect.x0 = 0.41f*w0;
                else
                    rect.x0 = 0.68f*w0;
                rect.x1 = rect.x0 + w;

                rect.z0 = -0.3333f * h0 + 0.25f*h;
                rect.z1 = rect.z0 - h;

                rect.Extend(1.0f, 0.85f);
            } break;

        case DRAW_UI_SWITCH_VR_ANGLE: {
                float const w0 = thumbnailRectangles_[0].Width();
                float const h0 = thumbnailRectangles_[0].Height();
                float const w = 0.48f*w0;
                float const h = 0.12f*h0;
                rect.x0 = 0.41f*w0;
                rect.x1 = rect.x0 + w;
                rect.z0 = -0.6f * h0 + 0.5f*h;
                rect.z1 = rect.z0 - h;
                rect.Extend(1.0f, 0.95f);//0.85f);
            } break;

        default:
            break;
        }

        return true;
    }
    else if (DRAW_TEXT_NO_VIDEOS==g) {
        TexCoord const& tc = texcoords_[DRAW_TEXT_NO_VIDEOS];
        float h = 0.75f;
        float w = h*tc.AspectRatio();
        rect.x0 = -0.5f*w;
        rect.x1 = rect.x0 + w;
        rect.z0 = 0.65f*h;
        rect.z1 = rect.z0 - h;
        return true;
    }
    else if (DRAW_TEXT_PUT_VIDEOS_IN_PATH==g) {
        TexCoord const& tc = texcoords_[DRAW_TEXT_PUT_VIDEOS_IN_PATH];
        float h = 0.66f;
        float w = h*tc.AspectRatio();
        if (w>0.95f*dashboard_width_) {
            w = 0.95f*dashboard_width_;
            h = w/tc.AspectRatio();
        }
        rect.x0 = -0.5f*w;
        rect.x1 = rect.x0 + w;
        rect.z0 = -0.7f;
        rect.z1 = rect.z0 - h;
        return true;
    }
    else if (DRAW_TEXT_3D==g) {
        float const h0 = thumbnailRectangles_[0].Height();
        float const h = 0.16f*h0;// 0.12f*h0;
        float const w = h*texcoords_[DRAW_TEXT_3D].AspectRatio();
        rect.x0 = 0.075f*thumbnailRectangles_[0].Width();
        rect.x1 = rect.x0 + w;
        rect.z0 = -0.3333f * h0 + 0.3f*h;
        rect.z1 = rect.z0 - h;
        return (NULL==current_track_);
    }
    else if (DRAW_TEXT_VR==g) {
        float const h0 = thumbnailRectangles_[0].Height();
        float const h = 0.16f*h0;
        float const w = h*texcoords_[DRAW_TEXT_VR].AspectRatio();
        rect.x0 = 0.075f*thumbnailRectangles_[0].Width();
        rect.x1 = rect.x0 + w;
        rect.z0 = -0.6f * h0 + 0.5f*h;
        rect.z1 = rect.z0 - h;
        return (NULL==current_track_);
    }
    return false;
}
//---------------------------------------------------------------------------------------
bool VRVideoPlayer::GetLanguageSubtitleRect_(Rectangle& rect, DRAW_GLYPH g, int item) const
{
    float const very_top = 0.5f*dashboard_height_ - 0.15f*widget_height_;
    float const base = widget_top_ + 0.05f*widget_height_;
    float const unit_height = 0.3f*widget_height_; // try me!
    float const space_align = 0.25f*unit_height;

    if (!GetUIWidgetRect_(rect, g))
        return false;

    // window size
    rect.Extend(3.0f, 1.0f);

    float const window_width = rect.Width();
    float const column_width = 0.965f*window_width;

    // maximum items in a column
    int const max_colume_items = (int) floor((very_top-base-2.0f*space_align)/unit_height);
    int total_items = 0;
    if (DRAW_UI_LANGUAGE==g && item<total_audio_streams_) {
        total_items = total_audio_streams_;
    }
    else if (DRAW_UI_SUBTITLE==g && item<=total_subtitle_streams_) {
        total_items = total_subtitle_streams_ + 1; // extra item for "Subtitle Off"
    }

    if (total_items>0) {
        int const columns = (total_items+max_colume_items-1)/max_colume_items;
        int const rows = (total_items+columns-1)/columns;
        float const top = base + unit_height*rows + space_align*2;
        float const left = rect.x1 - window_width - (columns-1)*column_width;
        if (0<=item) {
            if ((item+rows)>=total_items) {
                rect.z1 = base + unit_height*(total_items-item-1) + space_align;
                rect.z0 = rect.z1 + unit_height;
            }
            else {
                rect.x0 -= column_width;
                item = total_items - rows - item - 1; // reverse order
                int empty_slots = columns * rows - total_items;
                while ((item+(empty_slots>0))>=rows) {
                    rect.x0 -= column_width;
                    item -= (empty_slots>0) ? (rows-1):rows;
                    --empty_slots;
                }

                if (empty_slots>0) ++item;
                rect.z1 = base + unit_height*item + space_align;
                rect.z0 = rect.z1 + unit_height;
                rect.x1 = rect.x0 + window_width;
            }

            rect.Extend(0.95f, 0.95f);
        }
        else {
            rect.x0 = left;
            rect.z0 = top;
            rect.z1 = base;
        }
        return true;
    }

    return false;
}
//---------------------------------------------------------------------------------------
bool VRVideoPlayer::MenuUpdate_(mlabs::balai::VR::TrackedDevice const* prev_ctrl)
{
    float const current_time = (float) mlabs::balai::system::GetTime();
    int const total_videos = playList_.size();
    int const total_pages = (total_videos + MENU_VIDEO_THUMBNAILS - 1)/MENU_VIDEO_THUMBNAILS;
    int const current_page = menu_page_;

    if (menu_auto_scroll_) {
        float const fl = floorf(menu_page_scroll_);
        float const ce = ceilf(menu_page_scroll_);
        float const elapsed_time = current_time - event_timestamp_;
        event_timestamp_ = current_time;
        //
        // simple math: elapsed time is about 11.11ms or 0.011 sec.
        // for max_speed = base_speed + acc_speed = 15.0. so the max travel distance
        // is about 0.15 page for a single frame. means 7 frames to scroll a page.
        // that's pretty fast(but no faster!)
        float const base_speed = 2.5;
        float const acc_speed = 12.5f;
        if (2==menu_auto_scroll_ || (3!=menu_auto_scroll_ && ((menu_page_scroll_-fl)<(ce-menu_page_scroll_)))) {
            float speed = menu_page_scroll_ - fl; // traveling distance
            speed = base_speed + speed*speed*acc_speed;
            if ((menu_page_scroll_-=speed*elapsed_time)<=fl) {
                assert(0.0f==fl || -1.0f==fl);
                menu_auto_scroll_ = 0;
                menu_page_scroll_ = 0.0f;
                if (-1.0f==fl) {
                    menu_page_ = (menu_page_+1)%total_pages;
                }
            }
        }
        else {
            float speed = ce - menu_page_scroll_; // traveling distance
            speed = base_speed + speed*speed*acc_speed;
            if ((menu_page_scroll_+=speed*elapsed_time)>=ce) {
                assert(0.0f==ce || 1.0f==ce);
                menu_auto_scroll_ = 0;
                menu_page_scroll_ = 0.0f;
                if (1.0f==ce) {
                    menu_page_ = (menu_page_ + total_pages - 1)%total_pages;
                }
            }
        }
    }

    if (current_page!=menu_page_) {
        pageCV_.notify_all();
    }

    // standby time...
    if (widget_timer_>current_time) {
        return true;
    }

    if (0<widget_on_) {
        if (2==widget_on_) {
            if (current_time>subevent_timestamp_) {
                widget_on_ = 1; // fade out
                widget_timer_ = current_time;
            }
        }
        else {
            float const elapsed_time = current_time - widget_timer_;
            if (animation_duration_<elapsed_time) {
                if (2<widget_on_) {
                    widget_on_ = 2;
                    widget_timer_ = current_time - animation_duration_; // 100% faded in
                }
                else {
                    widget_on_ = 0; // off
                }
            }
        }
    }

    if (NULL!=focus_controller_) {
        mlabs::balai::VR::ControllerState curr_state, prev_state;
        focus_controller_->GetControllerState(curr_state, &prev_state);
        
        bool const on_trigger = curr_state.Trigger>0.0f && (prev_state.Trigger<=0.0f || prev_ctrl!=focus_controller_);

        if (curr_state.Menu!=0 && (0==prev_state.Menu || prev_ctrl!=focus_controller_)) {
            ResetDashboardPose_();// reposition
        }

        if (total_videos<1) {
            if (on_trigger) {
                float hit_x, hit_z;
                if (0.0f<DashboardPickTest_(hit_x, hit_z, focus_controller_)) {
                    Rectangle rect;
                    GetUIWidgetRect_(rect, DRAW_TEXT_NO_VIDEOS);
                    rect.Extend(1.05f, 1.05f);
                    if (rect.In(hit_x, hit_z)) {
                        quit_count_down_ = 90;
                    }
                }
            }
            return true;
        }

        if (0.0f==menu_page_scroll_) {
            float hit_x, hit_z;
            if (0<=menu_picking_slot_) {
                Matrix3 xform;
                OnSelectedThumbnailTransform_(xform, menu_picking_slot_, current_time-event_timestamp_);
                int const old_picking_slot_ = menu_picking_slot_;
                menu_picking_slot_ = -1;
                if (0.0f<PickTest(hit_x, hit_z, xform, focus_controller_)) {
                    Rectangle const& rect = thumbnailRectangles_[old_picking_slot_];
                    if (rect.In(hit_x, hit_z)) {
                        int const id = menu_page_*MENU_VIDEO_THUMBNAILS + old_picking_slot_;
                        if (id<(int)playList_.size()) {
                            VideoTrack* video = playList_[id];
                            menu_picking_slot_ = old_picking_slot_;

                            VIDEO_3D const s3D = video->Stereoscopic3D();
                            bool const is3D = (VIDEO_3D_MONO!=s3D);
                            bool const isTB = (0!=(s3D&VIDEO_3D_TOPBOTTOM));
                            bool const isVR = video->IsSpherical();

                            Rectangle r;
                            GetUIWidgetRect_(r, DRAW_UI_SETTINGS);
                            r.Move(rect.x0, rect.z0);
                            bool tweaking = r.In(hit_x, hit_z);

                            if (widget_on_>1) {
                                GetUIWidgetRect_(r, DRAW_UI_CONFIG_RECT);
                                r.Move(rect.x0, rect.z0);
                                if (r.In(hit_x, hit_z)) {
                                    tweaking = true;
                                    if (2==widget_on_ && (widget_timer_+animation_duration_)<current_time) {
                                        int longi(0), lat_inf(0), lati_sup(0);
                                        if (isVR) {
                                            video->GetSphericalAngles(longi, lat_inf, lati_sup);
                                        }
                                        bool const trigger_vibrate = (trigger_dashboard_align_==3);
                                        uint8 const last_trigger = trigger_dashboard_;
                                        trigger_dashboard_ = 2;

                                        DRAW_GLYPH const ui_widgets[] = {
                                            DRAW_UI_SWITCH_3D,
                                            DRAW_UI_SWITCH_VR,
                                            DRAW_UI_SWITCH_3D_SBS,
                                            DRAW_UI_SWITCH_3D_TB,
                                            DRAW_UI_SWITCH_VR_ANGLE
                                        };

                                        for (int ui=0; ui<sizeof(ui_widgets)/sizeof(ui_widgets[0]); ++ui) {
                                            DRAW_GLYPH const dg = ui_widgets[ui];
                                            GetUIWidgetRect_(r, dg);
                                            r.Move(rect.x0, rect.z0);

                                            ////////////////////////
                                            // ugly
                                            if (DRAW_UI_SWITCH_VR_ANGLE==dg) {
                                                r.x1 += 0.5f*r.Height();
                                            }
                                            ///////////////////////////

                                            if (r.In(hit_x, hit_z)) {
                                                if (DRAW_UI_SWITCH_3D==dg) {
                                                    if (trigger_vibrate) {
                                                        Rumble_VRController(focus_controller_);
                                                    }
                                                    trigger_dashboard_ = 3;
                                                }
                                                else if (DRAW_UI_SWITCH_VR==dg) {
                                                    if (trigger_vibrate) {
                                                        Rumble_VRController(focus_controller_);
                                                    }
                                                    trigger_dashboard_ = 5;
                                                }
                                                else if (DRAW_UI_SWITCH_3D_SBS==dg) {
                                                    if (isTB && trigger_vibrate) {
                                                        Rumble_VRController(focus_controller_);
                                                    }
                                                    trigger_dashboard_ = 4;
                                                }
                                                else if (DRAW_UI_SWITCH_3D_TB==dg) {
                                                    if (is3D && !isTB && trigger_vibrate) {
                                                        Rumble_VRController(focus_controller_);
                                                    }
                                                    trigger_dashboard_ = 8;
                                                }
                                                else if (DRAW_UI_SWITCH_VR_ANGLE==dg) {
                                                    r.Extend(1.0f, 0.75f);
                                                    if (r.In(hit_x, hit_z)) {
                                                        float const sr = 0.75f*r.Height();
                                                        if (fabs(r.x1-hit_x)<sr) {
                                                            if (isVR && 360!=longi && trigger_vibrate) {
                                                                Rumble_VRController(focus_controller_);
                                                            }
                                                            trigger_dashboard_ = 7;
                                                        }
                                                        else if (fabs(0.5f*(r.x0+r.x1)-hit_x)<sr) {
                                                            if (isVR && 180!=longi && trigger_vibrate) {
                                                                Rumble_VRController(focus_controller_);
                                                            }
                                                            trigger_dashboard_ = 6;
                                                        }
                                                    }
                                                }
                                                break;
                                            }
                                        }

                                        if (last_trigger==trigger_dashboard_) {
                                            if (trigger_dashboard_align_<127)
                                                ++trigger_dashboard_align_;
                                        }
                                        else {
                                            trigger_dashboard_align_ = 0;
                                        }

                                        if (on_trigger && 2!=trigger_dashboard_) {
                                            switch (trigger_dashboard_)
                                            {
                                            case 3:
                                                widget_on_ = 3;
                                                widget_timer_ = current_time;
                                                video->TweakVideo3D(is3D ? VIDEO_3D_MONO:VIDEO_3D_LEFTRIGHT);
                                                break;

                                            case 5:
                                                video->TweakVideoSpherical();
                                                widget_on_ = (isVR && 180==longi) ? 7:5;
                                                widget_timer_ = current_time;
                                                break;

                                            case 4:
                                            case 8:
                                                if (is3D) {
                                                    video->TweakVideo3D(isTB ? VIDEO_3D_LEFTRIGHT:VIDEO_3D_TOPBOTTOM);
                                                }
                                                break;

                                            case 6:
                                                if (isVR && 180!=longi) {
                                                    video->TweakVideoSphericalLongitudeSpan(180);
                                                    widget_on_ = 6;
                                                    widget_timer_ = current_time;
                                                }
                                                break;

                                            case 7:
                                                if (isVR && 360!=longi) {
                                                    video->TweakVideoSphericalLongitudeSpan(360);
                                                    widget_on_ = 6;
                                                    widget_timer_ = current_time;
                                                }
                                                break;

                                            default:
                                                break;
                                            }
                                        }
                                    }
                                }

                                // extend life time
                                if (2==widget_on_) {
                                    if (tweaking) {
                                        subevent_timestamp_ = current_time + animation_duration_;
                                    }
                                }
                            }
                            else { // widget_on_ = 1 or 0
                                if (tweaking) {
                                    Rumble_VRController(focus_controller_);
                                    widget_on_ = trigger_dashboard_ = 2;
                                    trigger_dashboard_align_ = 0;
                                    widget_timer_ = current_time;
                                    subevent_timestamp_ = current_time + animation_duration_;
                                }
                            }

                            if (!tweaking && on_trigger) {
#if 0
                                menu_picking_slot_ = -1;
                                widget_on_ = trigger_dashboard_ = trigger_dashboard_align_ = 0;
                                if (!Playback_(video)) {
                                    BL_ERR("failed to playback video... should be removed!\n");
                                    //
                                    // remove video or just set failed!?
                                    //
                                    video->SetFail();
                                }
#else
                                Playback_(video);
#endif
                            }
                        }
                    }
                }
            }
            else {
                menu_picking_slot_ = DashboardPickThumbnail_(focus_controller_);
                if (menu_picking_slot_>=0) {
                    if ((menu_page_*MENU_VIDEO_THUMBNAILS + menu_picking_slot_)<(int)playList_.size()) {
                        Rumble_VRController(focus_controller_);
                        event_timestamp_ = current_time;
                    }
                    else {
                        menu_picking_slot_ = -1;
                    }
                }
                widget_on_ = 0;
            }
        }
        else {
            menu_picking_slot_ = -1;
            trigger_dashboard_ = widget_on_ = 0;
        }

        // scroll pages
        if (total_pages>1 && menu_auto_scroll_<=1) {
            if (curr_state.OnTouchpad) {
                if (curr_state.OnTouchpad>1) { // pressed
                    if (prev_state.OnTouchpad<2 || prev_ctrl!=focus_controller_) {
                        //BL_LOG("press (%.3f, %.3f), prev_state %.3f\n", curr_state.Touchpad.x, curr_state.Touchpad.y, prev_state.Touchpad.x);
                        if (curr_state.Touchpad.x<-0.1f) {
                            menu_auto_scroll_ = 3;
                            event_timestamp_ = current_time;
                            //if (menu_page_scroll_==0.0f)
                            if (fabs(menu_page_scroll_)<0.1f)
                                menu_page_scroll_ = 0.01f;
                        }
                        else if (curr_state.Touchpad.x>0.1f) {
                            menu_auto_scroll_ = 2;
                            event_timestamp_ = current_time;
                            //if (menu_page_scroll_==0.0f)
                            if (fabs(menu_page_scroll_)<0.1f)
                                menu_page_scroll_ = -0.01f;
                        }
                    }
                }
                else if (prev_state.OnTouchpad) { // continue touching
                    menu_auto_scroll_ = 0; // cancelled
                    float slide = curr_state.Touchpad.x - prev_state.Touchpad.x;
                    //
                    // andre 2016/04/16:
                    // experence shows the fastest slide will be around 0.5.
                    // occasionally you may have a very big value like 1.5,
                    // but i consider it's a hardware hiccup.
                    /*
                    static float fast_move = fabs(slide);
                    static float slow_move = 100.0f;
                    float const move = fabs(slide);
                    if (fast_move<move) {
                        BL_LOG("fast move = %.4f\n", fast_move=move);
                    }
                    else if (move>0.0f && move<slow_move) {
                        BL_LOG("slow move = %.4f\n", slow_move=move);
                    }
                    */

                    // clamp slide value
                    if (-0.01f<slide && slide<0.01f) {
                        slide = 0.0f; // noise
                    }
                    else if (slide>0.75f) {
                        slide = 0.75f;
                    }
                    else if (slide<-0.75f) {
                        slide = -0.75f;
                    }
                    else if (fabs(menu_page_scroll_)<0.1f && fabs(slide)<0.05f) {
                        slide = 0.0f;
                    }

                    menu_page_scroll_ += 0.6f*slide;
                    while (menu_page_scroll_>1.0f) {
                        menu_page_ = (menu_page_+total_pages-1)%total_pages;
                        menu_page_scroll_ -= 1.0f;
                    }
                    while (menu_page_scroll_<-1.0f) {
                        menu_page_ = (menu_page_+1)%total_pages;
                        menu_page_scroll_ += 1.0f;
                    }

                    if (current_page!=menu_page_) {
                        //BL_LOG("page : %d->%d\n", current_page, menu_page_);
                        event_timestamp_ = current_time;
                        pageCV_.notify_all();
                    }
                }
                else {
                    // touching just start
                }
            }
            else if (prev_state.OnTouchpad) { // untouch
                if (0.0f!=menu_page_scroll_) {
                    menu_auto_scroll_ = 1;
                    event_timestamp_ = current_time;
                }
            }
        } // page scroll by touching
    }
    else { // focus_controller_ = NULL
        menu_picking_slot_ = -1;
        widget_on_ = 0;
        if (NULL!=prev_ctrl) {
            if (0.0f!=menu_page_scroll_ && 0==menu_auto_scroll_) {
                menu_auto_scroll_ = 1;
                event_timestamp_ = current_time;
            }
        }
    }

    return true;
}
//---------------------------------------------------------------------------------------
bool VRVideoPlayer::DrawMainMenu_(mlabs::balai::VR::HMD_EYE eye) const
{
    float const current_time = (float) mlabs::balai::system::GetTime();

    // draw background
    Renderer& renderer = Renderer::GetInstance();

    renderer.PushState();

    // disable Z
    renderer.SetDepthWrite(false);
    renderer.SetZTestDisable();

    // draw spherical background
    Matrix3 view(Matrix3::Identity), pose;
    vrMgr_.GetHMDPose(pose, eye);
    pose.SetOrigin(0.0f, 0.0f, 0.0f);
    gfxBuildViewMatrixFromLTM(view, view.SetEulerAngles(0.0f, 0.0f, azimuth_adjust_)*pose);
    renderer.PushViewMatrix(view);
    renderer.SetEffect(pan360_);
    float const crop_factor[4] = { 0.5f, // center
                             0.15915494309189533576888376337251f,   // 1/2pi
                             0.0f, // map to latitude top
                             0.31830988618379067153776752674503f }; // 1/pi
    float const diffuse[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    pan360_->BindConstant(pan360Crop_, crop_factor);
    pan360_->BindConstant(pan360Diffuse_, diffuse);
    pan360_->BindSampler(pan360Map_, background_);
    renderer.CommitChanges();
    fullsphere_.Drawcall();
    renderer.PopViewMatrix();

    Matrix3 dashboard_aniamtion_xform = dashboard_xform_;
    if (current_time<widget_timer_) {
        float s = (widget_timer_-current_time)/menu_animation_duration_;
        s = 1.0f - s*s;
        Matrix3 scale(Matrix3::Identity);
        scale._11 = scale._22 = scale._33 = (s<0.01f) ? 0.01f:s;
        dashboard_aniamtion_xform *= scale;
    }

    Primitives& prim = Primitives::GetInstance();
    Color color;

    // draw dashboard base
    renderer.SetBlendMode(GFXBLEND_SRCALPHA, GFXBLEND_INVSRCALPHA);
    renderer.SetWorldMatrix(dashboard_aniamtion_xform);
    renderer.SetCullDisable();
    if (prim.BeginDraw(NULL, GFXPT_QUADLIST)) {
        float const dw = 0.5f*dashboard_width_;
        float const dh = 0.5f*dashboard_height_;
        color = viveColor_; color.a = 64;
        prim.SetColor(color);
        prim.AddVertex(-dw, 0.0f, dh);
        prim.AddVertex(-dw, 0.0f, -dh);
        prim.AddVertex(dw, 0.0f, -dh);
        prim.AddVertex(dw, 0.0f, dh);
        prim.EndDraw();
    }

    // enable Z write
    renderer.SetBlendDisable();
    renderer.SetDepthWrite(true);
    //renderer.SetZTest();

    ///////////////////////////////////////////////
    // test
#if 0
    {
        ITexture* tex = vrWidget_.GetTexture();
        if (NULL!=tex) {
            float const ww = 10.0f;
            float const hh = ww*tex->Height()/tex->Width();

            renderer.SetWorldMatrix(Matrix3::Identity);
            prim.BeginDraw(tex, GFXPT_QUADLIST);

            prim.AddVertex(5.0f, 0.5f*ww, 0.5f*hh);
            prim.AddVertex(5.0f, 0.5f*ww, -0.5f*hh);
            prim.AddVertex(5.0f, -0.5f*ww, -0.5f*hh);
            prim.AddVertex(5.0f, -0.5f*ww, 0.5f*hh);

            prim.EndDraw();

            tex->Release();
            renderer.SetWorldMatrix(dashboard_xform_);
        }
    }
#endif
    //////////////////////////////////////////////

    int const total_videos = (int) playList_.size();
    if (total_videos<1) {
        if (0==quit_count_down_) {
            renderer.PopState();
            return false;
        }

        Rectangle rect;
        GetUIWidgetRect_(rect, DRAW_TEXT_NO_VIDEOS);
        Vector3 hit[2];
        float dists[2];
        Vector3 linelist[4];
        int hit_points = 0;
        int line_points = 0;
        bool hilight = false;
        for (int i=0; i<2; ++i) {
            mlabs::balai::VR::TrackedDevice const* device = vrMgr_.GetTrackedDevice(i);
            if (device && device->IsTracked()) {
                Vector3& h = hit[hit_points];
                float const dist = DashboardPickTest_(h.x, h.z, device);
                Matrix3 const xform = dashboard_aniamtion_xform.GetInverse() * device->GetPose();
                linelist[line_points++] = xform.Origin();
                if (0.0f<dist) {
                    h.y = -0.001f;
                    dists[hit_points++] = dist;
                    linelist[line_points++] = xform.PointTransform(Vector3(0.0f, dist, 0.0f));

                    if (!hilight)
                        hilight = rect.In(h.x, h.z);
                }
                else {
                    linelist[line_points++] = xform.PointTransform(Vector3(0.0f, 10.0f, 0.0f));
                }
            }
        }

        renderer.SetWorldMatrix(dashboard_aniamtion_xform);
        renderer.SetBlendMode(GFXBLEND_SRCALPHA, GFXBLEND_INVSRCALPHA);

        uint8 alpha_fade = 255;
        if (quit_count_down_>0 && quit_count_down_<90) {
            alpha_fade = (uint8) (255*quit_count_down_/90);
        }

        if (hilight || quit_count_down_>0) {
            prim.BeginDraw(NULL, GFXPT_QUADLIST);
            color = Color::Black; color.a = alpha_fade;
            prim.SetColor(color);
            prim.AddVertex(rect.x0, 0.0f, rect.z0);
            prim.AddVertex(rect.x0, 0.0f, rect.z1);
            prim.AddVertex(rect.x1, 0.0f, rect.z1);
            prim.AddVertex(rect.x1, 0.0f, rect.z0);
            prim.EndDraw();
        }

        renderer.SetEffect(fontFx_);
        fontFx_->SetSampler(shader::semantics::diffuseMap, fonts_[0]);
        prim.BeginDraw(GFXPT_QUADLIST);
        {
            color = Color::Yellow;
            GetUIWidgetRect_(rect, DRAW_TEXT_NO_VIDEOS);
            TexCoord tc = texcoords_[DRAW_TEXT_NO_VIDEOS];
            if (quit_count_down_>0) {
                TexCoord const& tc2 = texcoords_[DRAW_TEXT_APP_WILL_QUIT];
                rect.Extend(tc2.AspectRatio()/tc.AspectRatio(), 1.0f);
                tc = tc2;
                color.a = alpha_fade;
            }
            prim.SetColor(color);
            prim.AddVertex(rect.x0, 0.0f, rect.z0, tc.x0, tc.y0);
            prim.AddVertex(rect.x0, 0.0f, rect.z1, tc.x0, tc.y1);
            prim.AddVertex(rect.x1, 0.0f, rect.z1, tc.x1, tc.y1);
            prim.AddVertex(rect.x1, 0.0f, rect.z0, tc.x1, tc.y0);

            prim.SetColor(Color::White);
            GetUIWidgetRect_(rect, DRAW_TEXT_PUT_VIDEOS_IN_PATH);
            tc = texcoords_[DRAW_TEXT_PUT_VIDEOS_IN_PATH];
            prim.AddVertex(rect.x0, 0.0f, rect.z0, tc.x0, tc.y0);
            prim.AddVertex(rect.x0, 0.0f, rect.z1, tc.x0, tc.y1);
            prim.AddVertex(rect.x1, 0.0f, rect.z1, tc.x1, tc.y1);
            prim.AddVertex(rect.x1, 0.0f, rect.z0, tc.x1, tc.y0);
        }
        prim.EndDraw();

        if (line_points>0) {
            prim.BeginDraw(uiGlyph_, GFXPT_QUADLIST);
            prim.SetColor(viveColor_);
            TexCoord const& tc = texcoords_[DRAW_UI_CIRCLE];
            for (int i=0; i<hit_points; ++i) {
                Vector3 const& h = hit[i];
                float const sh = 0.5f * 0.3f*sqrt(dists[i]/dashboard_distance_); // roughly compensate
                float const sw = sh*tc.AspectRatio();
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
        }

        if (line_points>0) {
            prim.BeginDraw(NULL, GFXPT_LINELIST);
            prim.SetColor(viveColor_);
            for (int i=0; i<line_points; ++i) {
                prim.AddVertex(linelist[i]);
            }
            prim.EndDraw();
        }

        renderer.SetZTest();
        for (int i=0; i<2; ++i) {
            mlabs::balai::VR::TrackedDevice const* device = vrMgr_.GetTrackedDevice(i);
            if (device) {
                device->DrawSelf();
            }
        }

        renderer.PopState();

        return true;
    } // no videos

    //
    // draw all video thumbnails, video info(360/3D), widget UI...
    //

    int const total_pages = (total_videos+MENU_VIDEO_THUMBNAILS-1)/MENU_VIDEO_THUMBNAILS;
    float const left_most = thumbnailRectangles_[0].x0;
    float const right_most = thumbnailRectangles_[MENU_VIDEO_THUMBNAIL_COLUMN-1].x1;
    float const thumbnail_aspect_ratio = thumbnailRectangles_[0].AspectRatio();
    float const page_width = right_most - left_most + 1.75f*(thumbnailRectangles_[1].x0 - thumbnailRectangles_[0].x1);
    float const scroll = menu_page_scroll_ * page_width;
    float const page_offsets[2] = { 0.0f, (scroll>0.0f) ? -page_width:page_width };
    int const page_indices[2] = { menu_page_, (menu_page_ + (scroll>0.0f ? (total_pages-1):1))%total_pages };

    // hit points...
    struct HitPoint {
        Vector3 pt;
        Color  clr;
        float dist;
    } hit_points[2];
    int total_hit_points = 0;

    // tag (360/180/3D)
    Rectangle decal;
    struct TextRect {
        Rectangle rt;
        TexCoord  tc;
    } tags[MENU_VIDEO_THUMBNAILS*2*3]; // max 2 pages, 3 tags for each thumbnail.
    int tags_count = 0;

    ITexture* tex = NULL;
    float const elapsed_time = current_time - event_timestamp_;
    float s0(0.0f), t0(0.0f), s1(0.0f), t1(0.0f), x0, x1;
    int longi(0), lat_inf(0), lati_sup(0);

    // (non-picking) video thumbnails
    int const check_pages = (0.0f==menu_page_scroll_) ? 1:2;
    for (int k=0; k<check_pages; ++k) {
        float const offset = page_offsets[k] + scroll;
        int const base_index = page_indices[k]*MENU_VIDEO_THUMBNAILS;
        for (int i=0; i<MENU_VIDEO_THUMBNAILS; ++i) {
            Rectangle const& rect = thumbnailRectangles_[i];
            int const id = base_index + i;
            if (id<total_videos) {
                VideoTrack const* video = playList_[id];
                x0 = rect.x0 + offset;
                x1 = rect.x1 + offset;
                if (x0>=right_most || x1<=left_most || (0==k && menu_picking_slot_==i))
                    continue;

                int const tid = video->ThumbnailTextureId();
                tex = (0<=tid && tid<MENU_VIDEO_THUMBNAIL_TEXTURES) ? videoThumbnails_[tid]:NULL;
                prim.BeginDraw(tex, GFXPT_QUADLIST);
                    if (NULL!=tex) {
                        float const aspect_ratio = video->AspectRatio();
                        VIDEO_3D const s3D = video->Stereoscopic3D();
                        s0 = t0 = 0.0f;
                        s1 = t1 = 0.5f;
                        if (VIDEO_3D_MONO!=s3D) {
                            if (0==(s3D&VIDEO_3D_TOPBOTTOM)) { // SBS
                                if ((mlabs::balai::VR::HMD_EYE_LEFT==eye) ^ (VIDEO_3D_LEFTRIGHT==s3D)) {
                                    s0 = 0.5f*(s0+s1);
                                }
                                else {
                                    s1 = 0.5f*(s0+s1);
                                }
                            }
                            else { // TB
                                if ((mlabs::balai::VR::HMD_EYE_LEFT==eye) ^ (VIDEO_3D_TOPBOTTOM==s3D)) {
                                    t0 = 0.5f*(t0+t1);
                                }
                                else {
                                    t1 = 0.5f*(t0+t1);
                                }
                            }

                            //
                            // extra trim for 360 videos - Omnidirectional stereo (ODS) is NOT
                            // truly parallax.
                            //
                            // TO-DO : Precisely(in mathematical sense), we should draw
                            //         (hemi-) sphere here.
                            //
                            if (video->GetSphericalAngles(longi, lat_inf, lati_sup)) {
                                float const crop = (longi>180) ? 0.66f:0.8f;
                                s0 = 0.5f*(s0+s1);
                                s1 = (s1-s0)*crop;
                                s0 -= s1;
                                s1 = s0 + 2.0f*s1;

                                t0 = 0.5f*(t0+t1);
                                t1 = (t1-t0)*crop;
                                t0 -= t1;
                                t1 = t0 + 2.0f*t1;
                            }
                        }

                        if (thumbnail_aspect_ratio>aspect_ratio) {
                            t0 = 0.5f*(t0+t1);
                            t1 = (t1-t0)*aspect_ratio/thumbnail_aspect_ratio;
                            t0 -= t1;
                            t1 = t0 + 2.0f*t1;
                        }
                        else {
                            s0 = 0.5f*(s0+s1);
                            s1 = (s1-s0)*thumbnail_aspect_ratio/aspect_ratio;
                            s0 -= s1;
                            s1 = s0 + 2.0f*s1;
                        }
                    }
                    else {
                        prim.SetColor(Color::Black);
                    }

                    // crop
                    if (x0<left_most) {
                        s0 += (s1-s0)*(left_most-x0)/(x1-x0);
                        x0 = left_most;
                    }
                    else if (x1>right_most) {
                        s1 = s0 + (s1-s0)*(right_most-x0)/(x1-x0);
                        x1 = right_most;
                    }

                    prim.AddVertex(x0, 0.0f, rect.z0, s0, t0);
                    prim.AddVertex(x0, 0.0f, rect.z1, s0, t1);
                    prim.AddVertex(x1, 0.0f, rect.z1, s1, t1);
                    prim.AddVertex(x1, 0.0f, rect.z0, s1, t0);
                prim.EndDraw();
            }
            else {
                break;
            }
        }
    }

    if (0.0f==menu_page_scroll_) {
        // must enable ztest
        renderer.SetZTest();

        Matrix3 thumbnail_xform;
        if (0<=menu_picking_slot_) {
            int id = menu_page_*MENU_VIDEO_THUMBNAILS + menu_picking_slot_;
            if (id<total_videos) {
                // picking thumbnails
                longi = lat_inf = lati_sup = 0;
                VideoTrack const* video = playList_[id];
                Rectangle const& rect = thumbnailRectangles_[menu_picking_slot_];
                Rectangle invalid_rect = rect;
                VIDEO_3D const s3D = video->Stereoscopic3D();
                bool const isSBS = (VIDEO_3D_LEFTRIGHT==s3D || VIDEO_3D_RIGHTLEFT==s3D);
                bool const isTB = (VIDEO_3D_MONO!=s3D) && !isSBS;
                bool const is360 = video->GetSphericalAngles(longi, lat_inf, lati_sup);
                bool const isValid = video->IsValid();
                int const index = ((int) floor(elapsed_time))%4;
                s0 = 0.5f*(index%2); s1 = s0 + 0.5f;
                t0 = 0.5f*(index/2); t1 = t0 + 0.5f;
                id = video->ThumbnailTextureId();

                // thumbnail transform
                if (isValid) {
                    OnSelectedThumbnailTransform_(thumbnail_xform, menu_picking_slot_, elapsed_time);
                }
                else {
                    OnSelectedThumbnailTransform_(thumbnail_xform, menu_picking_slot_, 1.0f);
                    float const anim_duration = 0.3f;
                    if (elapsed_time<anim_duration) {
                        Vector3 const shake_head = 0.1f*rect.Width()*thumbnail_xform.XAxis();
                        float const animation_duration_1 = 0.25f*anim_duration;
                        float const animation_duration_2 = 0.5f*anim_duration;
                        float const animation_duration_3 = animation_duration_1 + animation_duration_2;
                        float alpha = 0.0f;
                        if (elapsed_time<animation_duration_1) {
                            alpha = elapsed_time/animation_duration_1;
                            alpha = -alpha*alpha;
                        }
                        else if (elapsed_time<animation_duration_3) {
                            alpha = (elapsed_time-animation_duration_1)/animation_duration_2;
                            alpha = 2.0f*alpha*alpha - 1.0f;
                        }
                        else {
                            alpha = (elapsed_time-animation_duration_3)/animation_duration_1;
                            alpha = 1.0f - alpha*alpha;
                        }
                        thumbnail_xform.SetOrigin(thumbnail_xform.Origin() + alpha*shake_head);
                    }
                }
                renderer.SetWorldMatrix(thumbnail_xform);

                tex = (0<=id && id<MENU_VIDEO_THUMBNAIL_TEXTURES) ? videoThumbnails_[id]:NULL;
                float processing_animation_time = -1.0f;
                prim.BeginDraw(tex, GFXPT_QUADLIST);
                    if (NULL!=tex) {
                        float const aspect_ratio = video->AspectRatio();
                        if (VIDEO_3D_MONO!=s3D && 0==widget_on_) {
                            if (isSBS) {
                                if ((mlabs::balai::VR::HMD_EYE_LEFT==eye) ^ (VIDEO_3D_LEFTRIGHT==s3D)) {
                                    s0 = 0.5f*(s0+s1);
                                }
                                else {
                                    s1 = 0.5f*(s0+s1);
                                }
                            }
                            else { // TB
                                if ((mlabs::balai::VR::HMD_EYE_LEFT==eye) ^ (VIDEO_3D_TOPBOTTOM==s3D)) {
                                    t0 = 0.5f*(t0+t1);
                                }
                                else {
                                    t1 = 0.5f*(t0+t1);
                                }
                            }

                            if (is360) {
                                float const crop = (longi>180) ? 0.66f:0.8f;
                                s0 = 0.5f*(s0+s1);
                                s1 = (s1-s0)*crop;
                                s0 -= s1;
                                s1 = s0 + 2.0f*s1;

                                t0 = 0.5f*(t0+t1);
                                t1 = (t1-t0)*crop;
                                t0 -= t1;
                                t1 = t0 + 2.0f*t1;
                            }
                        }

                        if (thumbnail_aspect_ratio>aspect_ratio) {
                            t0 = 0.5f*(t0+t1);
                            t1 = (t1-t0)*aspect_ratio/thumbnail_aspect_ratio;
                            t0 -= t1;
                            t1 = t0 + 2.0f*t1;
                        }
                        else {
                            s0 = 0.5f*(s0+s1);
                            s1 = (s1-s0)*thumbnail_aspect_ratio/aspect_ratio;
                            s0 -= s1;
                            s1 = s0 + 2.0f*s1;
                        }
                    }
                    else {
                        processing_animation_time = current_time;
                        prim.SetColor(Color::Black);
                    }
                    prim.AddVertex(rect.x0, 0.0f, rect.z0, s0, t0);
                    prim.AddVertex(rect.x0, 0.0f, rect.z1, s0, t1);
                    prim.AddVertex(rect.x1, 0.0f, rect.z1, s1, t1);
                    prim.AddVertex(rect.x1, 0.0f, rect.z0, s1, t0);
                prim.EndDraw();

                //
                // overlay
                //
                float const y = -0.01f;
                renderer.PushState();
                renderer.SetBlendMode(GFXBLEND_SRCALPHA, GFXBLEND_INVSRCALPHA);
                renderer.SetZTestDisable();
                renderer.SetDepthWrite(false);

                prim.BeginDraw(NULL, GFXPT_QUADLIST);

                if (0x1000==on_processing_ && (on_processing_time_start_+0.25f)<=current_time) {
                    color.r = color.g = color.b = 0; color.a = 160;
                    prim.SetColor(color);
                    prim.AddVertex(rect.x0, 0.0f, rect.z0);
                    prim.AddVertex(rect.x0, 0.0f, rect.z1);
                    prim.AddVertex(rect.x1, 0.0f, rect.z1);
                    prim.AddVertex(rect.x1, 0.0f, rect.z0);
                    if (processing_animation_time<0.0f) {
                        processing_animation_time = current_time - (on_processing_time_start_+0.25f);
                    }
                }

                // Stereoscopic 3D mask
                if (VIDEO_3D_MONO!=s3D && 1<widget_on_) {
                    uint8 const mask_translucency = 72;
                    if (isTB) {
                        float const mid_z = 0.5f*(rect.z0+rect.z1);
                        color = Color::Red; color.a = mask_translucency;
                        prim.SetColor(color);
                        prim.AddVertex(rect.x0, 0.0f, rect.z0);
                        prim.AddVertex(rect.x0, 0.0f, mid_z);
                        prim.AddVertex(rect.x1, 0.0f, mid_z);
                        prim.AddVertex(rect.x1, 0.0f, rect.z0);

                        color = Color::Blue; color.a = mask_translucency;
                        prim.SetColor(color);
                        prim.AddVertex(rect.x0, 0.0f, mid_z);
                        prim.AddVertex(rect.x0, 0.0f, rect.z1);
                        prim.AddVertex(rect.x1, 0.0f, rect.z1);
                        prim.AddVertex(rect.x1, 0.0f, mid_z);
                    }
                    else {
                        float const mid_x = 0.5f*(rect.x0+rect.x1);
                        color = Color::Red; color.a = mask_translucency;
                        prim.SetColor(color);
                        prim.AddVertex(rect.x0, 0.0f, rect.z0);
                        prim.AddVertex(rect.x0, 0.0f, rect.z1);
                        prim.AddVertex(mid_x, 0.0f, rect.z1);
                        prim.AddVertex(mid_x, 0.0f, rect.z0);

                        color = Color::Blue; color.a = mask_translucency;
                        prim.SetColor(color);
                        prim.AddVertex(mid_x, 0.0f, rect.z0);
                        prim.AddVertex(mid_x, 0.0f, rect.z1);
                        prim.AddVertex(rect.x1, 0.0f, rect.z1);
                        prim.AddVertex(rect.x1, 0.0f, rect.z0);
                    }
                }

                // widget disable color
                Color const disable_color = { 128, 128, 128, 255 };
                Color const hilight_color = Color::White; // { viveColor_.r, viveColor_.g, viveColor_.b, 128 };
                float const widget_elapsed_time = current_time - widget_timer_;
                bool const widget_full_open = widget_on_>2 || ((2==widget_on_) && widget_elapsed_time>animation_duration_);
                float spherical_degree = is360 ? (360==longi ? 1.0f:0.5f):0.0f;
                if (5<=widget_on_ && widget_elapsed_time<animation_duration_) {
                    float alpha = widget_elapsed_time/animation_duration_;
                    spherical_degree = alpha*alpha;

                    if (5==widget_on_) { // 0 <--> 360
                        if (!is360)
                            spherical_degree = 1.0f - spherical_degree;
                    }
                    else if (6==widget_on_) { // 180 <--> 360
                        spherical_degree *= 0.5f;
                        if (360==longi)
                            spherical_degree = 0.5f + spherical_degree;
                        else
                            spherical_degree = 0.5f + 0.5f*(1.0f - spherical_degree);
                    }
                    else if (7==widget_on_) { // 180 -> 0
                        spherical_degree = 0.5f*(1.0f - spherical_degree);
                    }
                }

                if (widget_on_>0) {
                    //
                    // .................
                    //
                    /*
                    float const flush_animation_time = 1.0f;
                    float const elapsed_time = current_time - subevent_timestamp_;
                    if (0.0f<=elapsed_time && elapsed_time<flush_animation_time) {
                        float alpha = 1.0f - elapsed_time/flush_animation_time;
                        color.r = color.g = color.b = 255;
                        color.a = uint8(alpha*alpha*255.0f);
                        prim.SetColor(color);
                        prim.AddVertex(rect.x0, -0.01f, rect.z0);
                        prim.AddVertex(rect.x0, -0.01f, rect.z1);
                        prim.AddVertex(rect.x1, -0.01f, rect.z1);
                        prim.AddVertex(rect.x1, -0.01f, rect.z0);
                    }
                    */

                    GetUIWidgetRect_(decal, DRAW_UI_CONFIG_RECT);
                    decal.Move(rect.x0, rect.z0);

                    color = Color::White; color.a = 148;
                    if (widget_on_<3) {
                        if (widget_elapsed_time<animation_duration_) {
                            float alpha = widget_elapsed_time/animation_duration_;
                            alpha *= alpha;
                            if (1==widget_on_)
                                alpha = 1.0f - alpha;

                            if (alpha<1.0f) {
                                decal.x0 = decal.x1 - decal.Width()*alpha;
                                decal.z1 = decal.z0 - decal.Height()*alpha;
                            }
                        }
                        else if (1==widget_on_) {
                            decal.x0 = decal.x1;
                            decal.z1 = decal.z0;
                            color.a = 0;
                        }
                    }

                    if (color.a>0) {
                        prim.SetColor(color);
                        prim.AddVertex(decal.x0, y, decal.z0);
                        prim.AddVertex(decal.x0, y, decal.z1);
                        prim.AddVertex(decal.x1, y, decal.z1);
                        prim.AddVertex(decal.x1, y, decal.z0);
                    }

                    // 3D & VR
                    if (widget_full_open) {
                        prim.SetColor(isSBS ? viveColor_:((isTB && 4==trigger_dashboard_ && trigger_dashboard_align_>3) ? hilight_color:disable_color));
                        GetUIWidgetRect_(decal, DRAW_UI_SWITCH_3D_SBS);
                        decal.Move(rect.x0, rect.z0);
                        prim.AddVertex(decal.x0, y, decal.z0);
                        prim.AddVertex(decal.x0, y, decal.z1);
                        prim.AddVertex(decal.x1, y, decal.z1);
                        prim.AddVertex(decal.x1, y, decal.z0);

                        prim.SetColor(isTB ? viveColor_:((isSBS && 8==trigger_dashboard_ && trigger_dashboard_align_>3) ? hilight_color:disable_color));
                        GetUIWidgetRect_(decal, DRAW_UI_SWITCH_3D_TB);
                        decal.Move(rect.x0, rect.z0);
                        prim.AddVertex(decal.x0, y, decal.z0);
                        prim.AddVertex(decal.x0, y, decal.z1);
                        prim.AddVertex(decal.x1, y, decal.z1);
                        prim.AddVertex(decal.x1, y, decal.z0);

                        prim.SetColor(/*is360 ? color:*/disable_color);
                        GetUIWidgetRect_(decal, DRAW_UI_SWITCH_VR_ANGLE);
                        decal.Move(rect.x0, rect.z0);
                        decal.Extend(1.0f, 0.4f);
                        prim.AddVertex(decal.x0, y, decal.z0);
                        prim.AddVertex(decal.x0, y, decal.z1);
                        prim.AddVertex(decal.x1, y, decal.z1);
                        prim.AddVertex(decal.x1, y, decal.z0);
                    }
                }
                else {
                    if (video->ViewerDiscretionIsAdviced()) {
                        decal = rect;
                        decal.Extend(0.95f, 1.0f);
                        decal.z1 = decal.Width()/texcoords_[DRAW_TEXT_WARNING].AspectRatio();
                        decal.z0 = 0.5f*(rect.z0+rect.z1) + 0.5f*decal.z1;
                        decal.z1 = decal.z0 - decal.z1;
                        decal.Extend(1.01f, 1.2f);

                        prim.SetColor(Color::White);
                        prim.AddVertex(decal.x0, y, decal.z0);
                        prim.AddVertex(decal.x0, y, decal.z1);
                        prim.AddVertex(decal.x1, y, decal.z1);
                        prim.AddVertex(decal.x1, y, decal.z0);
                    }
                }

                // UI and font rendering states

                // decal go first
                float const dz = 0.15f*rect.Height(); // 0.125f*(rect.z0 - rect.z1);
                decal.x1 = decal.x0 = rect.x0;
                float z0 = rect.z0 - 0.02f*dz; // 0.02
                float z1 = z0 - dz;
                decal.z0 = z0;// + 0.02f*dz;
                decal.z1 = z1 + 0.1f*dz;
                tags_count = 0;
                x0 = rect.x0 + 0.2f*dz;
                if (is360) {
                    TextRect& t = tags[tags_count++];
                    t.tc = texcoords_[(180==longi) ? DRAW_TEXT_180:DRAW_TEXT_360];
                    t.rt.x0 = x0;
                    t.rt.x1 = x0 + dz*t.tc.AspectRatio();
                    t.rt.z0 = z0;
                    t.rt.z1 = z1;
                    decal.x1 = x0 = t.rt.x1 + 0.15f*dz;
                }
                if (VIDEO_3D_MONO!=s3D) {
                    TextRect& t = tags[tags_count++];
                    t.tc = texcoords_[DRAW_TEXT_3D];
                    t.rt.x0 = x0;
                    t.rt.x1 = x0 + dz*t.tc.AspectRatio();
                    t.rt.z0 = z0;
                    t.rt.z1 = z1;
                    decal.x1 = x0 = t.rt.x1 + 0.15f*dz;
                }

                if (decal.x1>decal.x0) {
                    color = Color::Black; color.a = 64;
                    prim.SetColor(color);
                    prim.AddVertex(decal.x0, y, decal.z0);
                    prim.AddVertex(decal.x0, y, decal.z1);
                    prim.AddVertex(decal.x1, y, decal.z1);
                    prim.AddVertex(decal.x1, y, decal.z0);
                }

                if (!isValid) {
                    float const hh = 0.168f * rect.Height();
                    float const ww = hh * texcoords_[DRAW_TEXT_ERROR].AspectRatio();
                    decal.x0 = 0.5f*(rect.x0+rect.x1) - 0.5f*ww;
                    decal.x1 = decal.x0 + ww;
                    decal.z0 = (0.33f*rect.z0+0.66f*rect.z1) + 0.5f*hh;
                    decal.z1 = decal.z0 - hh;
                    color = Color::Black; color.a = 128;
                    prim.SetColor(color);
                    prim.AddVertex(decal.x0, y, decal.z0);
                    prim.AddVertex(decal.x0, y, decal.z1);
                    prim.AddVertex(decal.x1, y, decal.z1);
                    prim.AddVertex(decal.x1, y, decal.z0);

                    invalid_rect = decal;
                    invalid_rect.Extend(0.95f, 0.92f);
                }
                prim.EndDraw();

                // font rendering
                renderer.SetDepthWrite(true);
                renderer.SetEffect(fontFx_);
                fontFx_->SetSampler(shader::semantics::diffuseMap, fonts_[0]);
                prim.BeginDraw(GFXPT_QUADLIST);

                for (int i=0; i<tags_count; ++i) {
                    TextRect& t = tags[i];
                    prim.AddVertex(t.rt.x0, y, t.rt.z0, t.tc.x0, t.tc.y0);
                    prim.AddVertex(t.rt.x0, y, t.rt.z1, t.tc.x0, t.tc.y1);
                    prim.AddVertex(t.rt.x1, y, t.rt.z1, t.tc.x1, t.tc.y1);
                    prim.AddVertex(t.rt.x1, y, t.rt.z0, t.tc.x1, t.tc.y0);
                }

                if (widget_full_open) { // 3D and VR
                    color.r = color.g = color.b = 160; color.a = 255;
                    TexCoord const& tc1 = texcoords_[DRAW_TEXT_3D];
                    GetUIWidgetRect_(decal, DRAW_TEXT_3D);
                    decal.Move(rect.x0, rect.z0);
                    prim.SetColor((VIDEO_3D_MONO!=s3D) ? viveColor_:disable_color);
                    prim.AddVertex(decal.x0, y, decal.z0, tc1.x0, tc1.y0);
                    prim.AddVertex(decal.x0, y, decal.z1, tc1.x0, tc1.y1);
                    prim.AddVertex(decal.x1, y, decal.z1, tc1.x1, tc1.y1);
                    prim.AddVertex(decal.x1, y, decal.z0, tc1.x1, tc1.y0);

                    TexCoord const& tcSBS = texcoords_[DRAW_TEXT_LR];
                    GetUIWidgetRect_(decal, DRAW_UI_SWITCH_3D_SBS);
                    decal.Move(rect.x0, rect.z0);
                    //decal.Extend(1.0f, 0.9f);
                    decal.x0 = 0.5f*(decal.x0 + decal.x1);
                    decal.x1 = decal.Height() * tcSBS.AspectRatio();
                    decal.x0 -= 0.5f*decal.x1;
                    decal.x1 += decal.x0;
                    prim.SetColor(Color::Black);
                    prim.AddVertex(decal.x0, y, decal.z0, tcSBS.x0, tcSBS.y0);
                    prim.AddVertex(decal.x0, y, decal.z1, tcSBS.x0, tcSBS.y1);
                    prim.AddVertex(decal.x1, y, decal.z1, tcSBS.x1, tcSBS.y1);
                    prim.AddVertex(decal.x1, y, decal.z0, tcSBS.x1, tcSBS.y0);

                    TexCoord const& tcTB = texcoords_[DRAW_TEXT_TB];
                    GetUIWidgetRect_(decal, DRAW_UI_SWITCH_3D_TB);
                    decal.Move(rect.x0, rect.z0);
                    //decal.Extend(1.0f, 0.9f);
                    decal.x0 = 0.5f*(decal.x0 + decal.x1);
                    decal.x1 = decal.Height() * tcTB.AspectRatio();
                    decal.x0 -= 0.5f*decal.x1;
                    decal.x1 += decal.x0;
                    prim.SetColor(Color::Black);
                    prim.AddVertex(decal.x0, y, decal.z0, tcTB.x0, tcTB.y0);
                    prim.AddVertex(decal.x0, y, decal.z1, tcTB.x0, tcTB.y1);
                    prim.AddVertex(decal.x1, y, decal.z1, tcTB.x1, tcTB.y1);
                    prim.AddVertex(decal.x1, y, decal.z0, tcTB.x1, tcTB.y0);

                    // VR
                    TexCoord const& tc2 = texcoords_[DRAW_TEXT_VR];
                    GetUIWidgetRect_(decal, DRAW_TEXT_VR);
                    decal.Move(rect.x0, rect.z0);
                    prim.SetColor(is360 ? viveColor_:disable_color);
                    prim.AddVertex(decal.x0, y, decal.z0, tc2.x0, tc2.y0);
                    prim.AddVertex(decal.x0, y, decal.z1, tc2.x0, tc2.y1);
                    prim.AddVertex(decal.x1, y, decal.z1, tc2.x1, tc2.y1);
                    prim.AddVertex(decal.x1, y, decal.z0, tc2.x1, tc2.y0);

                    // scale
                    TexCoord const& t180 = texcoords_[DRAW_TEXT_180];
                    GetUIWidgetRect_(decal, DRAW_UI_SWITCH_VR_ANGLE);
                    decal.Move(rect.x0, rect.z0);
                    float const sh = 0.8f*decal.Height();
                    float const p180 = 0.5f*(decal.x0 + decal.x1);
                    float const p360 = decal.x1;
                    decal.z0 = decal.z1 + 0.2f*sh;
                    decal.z1 = decal.z0 - sh;
                    decal.x1 = sh * t180.AspectRatio();
                    decal.x0 = p180 - 0.5f*decal.x1;
                    decal.x1 += decal.x0;

                    prim.SetColor(Color::Black);
                    prim.AddVertex(decal.x0, y, decal.z0, t180.x0, t180.y0);
                    prim.AddVertex(decal.x0, y, decal.z1, t180.x0, t180.y1);
                    prim.AddVertex(decal.x1, y, decal.z1, t180.x1, t180.y1);
                    prim.AddVertex(decal.x1, y, decal.z0, t180.x1, t180.y0);

                    TexCoord const& t360 = texcoords_[DRAW_TEXT_360];
                    decal.x1 = sh * t360.AspectRatio();
                    decal.x0 = p360 - 0.5f*decal.x1;
                    decal.x1 += decal.x0;
                    prim.SetColor(Color::Black);
                    prim.AddVertex(decal.x0, y, decal.z0, t360.x0, t360.y0);
                    prim.AddVertex(decal.x0, y, decal.z1, t360.x0, t360.y1);
                    prim.AddVertex(decal.x1, y, decal.z1, t360.x1, t360.y1);
                    prim.AddVertex(decal.x1, y, decal.z0, t360.x1, t360.y0);
                }
                else if (0==widget_on_) {
                    if (video->ViewerDiscretionIsAdviced()) {
                        TexCoord const& tc = texcoords_[DRAW_TEXT_WARNING];
                        decal = rect;
                        decal.Extend(0.95f, 1.0f);
                        decal.z1 = decal.Width()/tc.AspectRatio();
                        decal.z0 = 0.5f*(rect.z0+rect.z1) + 0.5f*decal.z1;
                        decal.z1 = decal.z0 - decal.z1;

                        prim.SetColor(Color::Red);
                        prim.AddVertex(decal.x0, y, decal.z0, tc.x0, tc.y0);
                        prim.AddVertex(decal.x0, y, decal.z1, tc.x0, tc.y1);
                        prim.AddVertex(decal.x1, y, decal.z1, tc.x1, tc.y1);
                        prim.AddVertex(decal.x1, y, decal.z0, tc.x1, tc.y0);
                    }
                }

                if (!isValid) {
                    TexCoord const& tc = texcoords_[DRAW_TEXT_ERROR];
                    prim.SetColor(Color::Yellow);
                    prim.AddVertex(invalid_rect.x0, y, invalid_rect.z0, tc.x0, tc.y0);
                    prim.AddVertex(invalid_rect.x0, y, invalid_rect.z1, tc.x0, tc.y1);
                    prim.AddVertex(invalid_rect.x1, y, invalid_rect.z1, tc.x1, tc.y1);
                    prim.AddVertex(invalid_rect.x1, y, invalid_rect.z0, tc.x1, tc.y0);
                }

                // file name
                if (0<=id && id<(int)fonts_.size()) {
                    TexCoord const& tc = video->GetFontTexCoord(id);
                    if (id!=0) {
                        prim.EndDraw();
#ifndef USE_SUBTITLE_FONT_SHADER
                        fontFx_->SetSampler(shader::semantics::diffuseMap, fonts_[id]);
#else
                        subtitleFx_->SetSampler(shader::semantics::diffuseMap, fonts_[id]);
#endif
                        prim.BeginDraw(GFXPT_QUADLIST);
                    }

                    float const hh = 0.12f*(rect.z0-rect.z1);
                    float const ww = hh*tc.AspectRatio();
                    z1 = rect.z1 + 0.1f*hh;
                    z0 = z1 + hh;
                    x0 = 0.5f*(rect.x0 + rect.x1) - 0.5f*ww;
                    x1 = x0 + ww;
                    prim.SetColor(Color::White);
                    prim.AddVertex(x0, y, z0, tc.x0, tc.y0);
                    prim.AddVertex(x0, y, z1, tc.x0, tc.y1);
                    prim.AddVertex(x1, y, z1, tc.x1, tc.y1);
                    prim.AddVertex(x1, y, z0, tc.x1, tc.y0);
                }
                prim.EndDraw();

                prim.BeginDraw(uiGlyph_, GFXPT_QUADLIST);
                {
                    TexCoord const& tc = texcoords_[DRAW_UI_SETTINGS];
                    GetUIWidgetRect_(decal, DRAW_UI_SETTINGS);
                    decal.Move(rect.x0, rect.z0);
                    if (widget_on_>1) {
                        prim.SetColor(viveColor_);
                    }
                    prim.AddVertex(decal.x0, y, decal.z0, tc.x0, tc.y0);
                    prim.AddVertex(decal.x0, y, decal.z1, tc.x0, tc.y1);
                    prim.AddVertex(decal.x1, y, decal.z1, tc.x1, tc.y1);
                    prim.AddVertex(decal.x1, y, decal.z0, tc.x1, tc.y0);
                }

                if (widget_full_open) {
                    TexCoord const& tc = texcoords_[DRAW_UI_SWITCH_BASE];
                    TexCoord const& tb = texcoords_[DRAW_UI_SWITCH_BUTTON];
                    GetUIWidgetRect_(decal, DRAW_UI_SWITCH_3D);
                    decal.Move(rect.x0, rect.z0);
                    prim.SetColor((VIDEO_3D_MONO!=s3D)? viveColor_:disable_color);
                    prim.AddVertex(decal.x0, y, decal.z0, tc.x0, tc.y0);
                    prim.AddVertex(decal.x0, y, decal.z1, tc.x0, tc.y1);
                    prim.AddVertex(decal.x1, y, decal.z1, tc.x1, tc.y1);
                    prim.AddVertex(decal.x1, y, decal.z0, tc.x1, tc.y0);

                    float dw = decal.Height()*tb.AspectRatio();
                    if (3==widget_on_ && widget_elapsed_time<animation_duration_) {
                        float alpha = widget_elapsed_time/animation_duration_;
                        alpha *= alpha;
                        if (VIDEO_3D_MONO==s3D)
                            alpha = 1.0f - alpha;

                        decal.x0 += alpha*(decal.Width() - dw);
                    }
                    else {
                        if (VIDEO_3D_MONO!=s3D)
                            decal.x0 = decal.x1 - dw;
                    }
                    decal.x1 = decal.x0 + dw;
                    prim.SetColor(Color::White);
                    prim.AddVertex(decal.x0, y, decal.z0, tb.x0, tb.y0);
                    prim.AddVertex(decal.x0, y, decal.z1, tb.x0, tb.y1);
                    prim.AddVertex(decal.x1, y, decal.z1, tb.x1, tb.y1);
                    prim.AddVertex(decal.x1, y, decal.z0, tb.x1, tb.y0);

                    // VR
                    GetUIWidgetRect_(decal, DRAW_UI_SWITCH_VR);
                    decal.Move(rect.x0, rect.z0);
                    prim.SetColor(is360? viveColor_:disable_color);
                    prim.AddVertex(decal.x0, y, decal.z0, tc.x0, tc.y0);
                    prim.AddVertex(decal.x0, y, decal.z1, tc.x0, tc.y1);
                    prim.AddVertex(decal.x1, y, decal.z1, tc.x1, tc.y1);
                    prim.AddVertex(decal.x1, y, decal.z0, tc.x1, tc.y0);

                    dw = decal.Height()*tb.AspectRatio();
                    if ((5==widget_on_ || 7==widget_on_) && widget_elapsed_time<animation_duration_) {
                        float alpha = widget_elapsed_time/animation_duration_;
                        alpha *= alpha;
                        if (!is360)
                            alpha = 1.0f - alpha;
                        decal.x0 += alpha*(decal.Width() - dw);
                    }
                    else {
                        if (is360)
                            decal.x0 = decal.x1 - dw;
                    }
                    decal.x1 = decal.x0 + dw;
                    prim.SetColor(Color::White);
                    prim.AddVertex(decal.x0, y, decal.z0, tb.x0, tb.y0);
                    prim.AddVertex(decal.x0, y, decal.z1, tb.x0, tb.y1);
                    prim.AddVertex(decal.x1, y, decal.z1, tb.x1, tb.y1);
                    prim.AddVertex(decal.x1, y, decal.z0, tb.x1, tb.y0);

                    // 2 circles - 180/360
                    GetUIWidgetRect_(decal, DRAW_UI_SWITCH_VR_ANGLE);
                    decal.Move(rect.x0, rect.z0);
                    decal.Extend(1.0f, 0.75f);
                    float const sw = decal.Height() * tb.AspectRatio();
                    float const p180 = 0.5f*(decal.x0+decal.x1);
                    float const p360 = decal.x1;

                    // 180
                    decal.x0 = p180 - 0.5f*sw;
                    decal.x1 = decal.x0 + sw;
                    prim.SetColor((is360 && (180==longi || (6==trigger_dashboard_ && trigger_dashboard_align_>3))) ? viveColor_:disable_color);
                    prim.AddVertex(decal.x0, y, decal.z0, tb.x0, tb.y0);
                    prim.AddVertex(decal.x0, y, decal.z1, tb.x0, tb.y1);
                    prim.AddVertex(decal.x1, y, decal.z1, tb.x1, tb.y1);
                    prim.AddVertex(decal.x1, y, decal.z0, tb.x1, tb.y0);

                    // 360
                    decal.x0 = p360 - 0.5f*sw;
                    decal.x1 = decal.x0 + sw;
                    prim.SetColor((is360 && (360==longi || (7==trigger_dashboard_ && trigger_dashboard_align_>3))) ? viveColor_:disable_color);
                    prim.AddVertex(decal.x0, y, decal.z0, tb.x0, tb.y0);
                    prim.AddVertex(decal.x0, y, decal.z1, tb.x0, tb.y1);
                    prim.AddVertex(decal.x1, y, decal.z1, tb.x1, tb.y1);
                    prim.AddVertex(decal.x1, y, decal.z0, tb.x1, tb.y0);
                }

                if (processing_animation_time>0.0f) {
                    TexCoord const& tc = texcoords_[DRAW_UI_BUSY];
                    float const size = 0.2f*(rect.Width() + rect.Height());
                    float const cx = 0.5f*(rect.x0+rect.x1);
                    float const cy = 0.5f*(rect.z0+rect.z1);
                    float x[4], z[4];
                    BusySquare(x[0], z[0], x[1], z[1], x[2], z[2], x[3], z[3],
                               size, processing_animation_time);
                    prim.AddVertex(cx+x[0], 0.0f, cy+z[0], tc.x0, tc.y0);
                    prim.AddVertex(cx+x[1], 0.0f, cy+z[1], tc.x0, tc.y1);
                    prim.AddVertex(cx+x[2], 0.0f, cy+z[2], tc.x1, tc.y1);
                    prim.AddVertex(cx+x[3], 0.0f, cy+z[3], tc.x1, tc.y0);
                }

                prim.EndDraw();

                // spherical degree animation - draw this bar late after 180/360 button
                if (widget_full_open && spherical_degree>0.0f) {
                    prim.BeginDraw(NULL, GFXPT_QUADLIST);
                    GetUIWidgetRect_(decal, DRAW_UI_SWITCH_VR_ANGLE);
                    decal.Move(rect.x0, rect.z0);
                    decal.Extend(1.0f, 0.4f);
                    decal.x1 = decal.x0 + decal.Width()*spherical_degree;
                    prim.SetColor(viveColor_);
                    prim.AddVertex(decal.x0, y, decal.z0);
                    prim.AddVertex(decal.x0, y, decal.z1);
                    prim.AddVertex(decal.x1, y, decal.z1);
                    prim.AddVertex(decal.x1, y, decal.z0);
                    prim.EndDraw();
                }

                renderer.PopState();
            }
        }

        // draw max 2 controllers aiming
        renderer.SetWorldMatrix(dashboard_xform_);
        prim.BeginDraw(NULL, GFXPT_LINELIST);
        prim.SetColor(viveColor_);
        Matrix3 const dashboard_inv_xform = dashboard_xform_.GetInverse();
        float hit_x, hit_z, hit_distance;
        if (0<=menu_picking_slot_) {
            Rectangle const& rect = thumbnailRectangles_[menu_picking_slot_];
            for (int i=0; i<2; ++i) {
                mlabs::balai::VR::TrackedDevice const* device = vrMgr_.GetTrackedDevice(i);
                if (device && device->IsTracked()) {
                    HitPoint& pt = hit_points[total_hit_points];
                    Matrix3 const xform = dashboard_inv_xform * device->GetPose();
                    bool test_failed = false;
                    hit_distance = PickTest(hit_x, hit_z, thumbnail_xform, device);
                    if (device==focus_controller_) {
                        pt.clr = viveColor_;
                        if (0.0f<hit_distance) {
                            prim.SetColor(pt.clr);
                            prim.AddVertex(xform.Origin());
                            prim.AddVertex(xform.PointTransform(Vector3(0.0f, hit_distance, 0.0f)));
                        }
                    }
                    else {
                        pt.clr = Color::Gray; pt.clr.a = 128;
                        test_failed = true;
                        if (0.0f<hit_distance && rect.In(hit_x, hit_z)) {
                            test_failed = false;
                        }

                        if (test_failed) {
                            hit_distance = DashboardPickTest_(hit_x, hit_z, device);
                            if (hit_distance<0.0f) {
                                prim.SetColor(viveColor_);
                                prim.AddVertex(xform.Origin());
                                prim.AddVertex(xform.PointTransform(Vector3(0.0f, 100.0f, 0.0f)));
                            }
                        }
                    }

                    if (0.0f<hit_distance) {
                        ++total_hit_points;
                        pt.dist = hit_distance;
                        pt.pt.x = hit_x;
                        pt.pt.y = -0.001f;
                        pt.pt.z = hit_z;
                        if (!test_failed) {
                            pt.pt += dashboard_inv_xform.PointTransform(thumbnail_xform.Origin());
                        }
                    }
                }
            }
        }
        else {
            for (int i=0; i<2; ++i) {
                mlabs::balai::VR::TrackedDevice const* device = vrMgr_.GetTrackedDevice(i);
                if (device && device->IsTracked()) {
                    hit_distance = DashboardPickTest_(hit_x, hit_z, device);
                    if (0.0f<hit_distance) {
                        HitPoint& p = hit_points[total_hit_points++];
                        p.pt.x = hit_x;
                        p.pt.y = -0.001f;
                        p.pt.z = hit_z;
                        p.clr = Color::Gray; p.clr.a = 128;
                        p.dist = hit_distance;

                        // to be coincidence, gray mark comes with no pointing line.
                        //prim.AddVertex(xform.Origin());
                        //prim.AddVertex(xform.PointTransform(Vector3(0.0f, hit_distance, 0.0f)));
                    }
                    else {
                        Matrix3 const xform = dashboard_inv_xform * device->GetPose();
                        prim.AddVertex(xform.Origin());
                        prim.AddVertex(xform.PointTransform(Vector3(0.0f, 100.0f, 0.0f)));
                    }
                }
            }
        }
        prim.EndDraw();
    }
    else { // rest position, page not scrolling.
        Matrix3 const dashboard_inv_xform = dashboard_xform_.GetInverse();
        Vector3 v[12];
        int count = 0;
        float hit_x, hit_z, dist;
        for (int i=0; i<2; ++i) {
            mlabs::balai::VR::TrackedDevice const* device = vrMgr_.GetTrackedDevice(i);
            if (device && device->IsTracked()) {
                dist = DashboardPickTest_(hit_x, hit_z, device);
                Matrix3 const xform = dashboard_inv_xform * device->GetPose();
                if (dist<0.0f) {
                    if (NULL==focus_controller_ || focus_controller_==device) {
                        v[count++] = xform.Origin();
                        v[count++] = xform.PointTransform(Vector3(0.0f, 100.0f, 0.0f));
                    }
                }
                else if (focus_controller_==device) {
                    v[count++] = xform.Origin();
                    v[count++] = xform.PointTransform(Vector3(0.0f, dist, 0.0f));

                    HitPoint& p = hit_points[total_hit_points++];
                    p.pt.x = hit_x;
                    p.pt.y = -0.001f;
                    p.pt.z = hit_z;
                    p.clr  = viveColor_;
                    p.dist = dist;
                }
            }
        }

        if (count>0) {
            prim.BeginDraw(NULL, GFXPT_LINELIST);
            prim.SetColor(viveColor_);
            for (int i=0; i<count; ++i)
                prim.AddVertex(v[i]);
            prim.EndDraw();
        }
    }

    // cursor points
    renderer.SetWorldMatrix(dashboard_aniamtion_xform);
    renderer.PushState();
    renderer.SetCullDisable();
    renderer.SetBlendMode(GFXBLEND_SRCALPHA, GFXBLEND_INVSRCALPHA);
    if (total_hit_points>0) {
        renderer.PushState();
        renderer.SetDepthWrite(false);
        //renderer.SetZTestDisable();
        prim.BeginDraw(uiGlyph_, GFXPT_QUADLIST);
        TexCoord const& tc = texcoords_[DRAW_UI_CIRCLE];
        for (int i=0; i<total_hit_points; ++i) {
            HitPoint const& pt = hit_points[i];
            prim.SetColor(pt.clr);
            Vector3 const& h = pt.pt;
            float const sh = 0.2f * sqrt(pt.dist/dashboard_distance_);
            float const sw = sh*tc.AspectRatio();
            x0 = h.x - 0.5f*sw;
            x1 = h.x + 0.5f*sw;
            float z0 = h.z + 0.5f*sh;
            float z1 = h.z - 0.5f*sh;
            prim.AddVertex(x0, h.y, z0, tc.x0, tc.y0);
            prim.AddVertex(x0, h.y, z1, tc.x0, tc.y1);
            prim.AddVertex(x1, h.y, z1, tc.x1, tc.y1);
            prim.AddVertex(x1, h.y, z0, tc.x1, tc.y0);
        }
        prim.EndDraw();

        renderer.PopState();
    }

    // video information -- print text "360" "3D"
    tags_count = 0;

    // decal go first
    renderer.SetDepthWrite(false);
    renderer.SetZTest();
    prim.BeginDraw(NULL, GFXPT_QUADLIST);
    float const y = -0.01f;
    {
        color = Color::Black; color.a = 64;
        prim.SetColor(color);
        for (int k=0; k<check_pages; ++k) {
            float const offset = page_offsets[k] + scroll;
            int const base_index = page_indices[k]*MENU_VIDEO_THUMBNAILS;
            for (int i=0; i<MENU_VIDEO_THUMBNAILS; ++i) {
                if (i==menu_picking_slot_)
                    continue;

                int const id = base_index + i;
                if (id<total_videos) {
                    VideoTrack const* video = playList_[id];
                    Rectangle const& rect = thumbnailRectangles_[i];
                    //
                    // following code should be coincident with picking one line 1961(search 0.168)
                    //
                    float const dz = 0.15f*(rect.z0 - rect.z1);
                    decal.x0 = rect.x0 + offset;
                    decal.x1 = rect.x1 + offset; // TBD
                    if (decal.x0>=right_most || decal.x1<=left_most)
                        continue;

                    float const z0 = rect.z0 - 0.02f*dz;
                    float const z1 = z0 - dz;
                    decal.z0 = z0;// + 0.125f*dz;
                    decal.z1 = z1 + 0.1f*dz;
                    decal.x1 = decal.x0; // invalid

                    x0 = decal.x0 + 0.2f*dz;
                    if (x0<right_most &&
                        video->GetSphericalAngles(longi, lat_inf, lati_sup)) {
                        TexCoord const& tc = texcoords_[(180==longi) ? DRAW_TEXT_180:DRAW_TEXT_360];
                        x1 = x0 + dz*tc.AspectRatio();
                        decal.x1 = x1 + 0.15f*dz;
                        if (x1>left_most) {
                            s0 = tc.x0;
                            s1 = tc.x1;
                            if (x0<left_most) {
                                s0 += (s1-s0)*(left_most-x0)/(x1-x0);
                                x0 = left_most;
                            }
                            else if (x1>right_most) {
                                s1 = s1 + (s1-s0)*(right_most-x0)/(x1-x0);
                                x1 = right_most;
                            }

                            TextRect& t = tags[tags_count++];
                            t.rt.x0 = x0;
                            t.rt.x1 = x1;
                            t.rt.z0 = z0;
                            t.rt.z1 = z1;
                            t.tc.x0 = s0;
                            t.tc.x1 = s1;
                            t.tc.y0 = tc.y0;
                            t.tc.y1 = tc.y1;

                            x0 = x1 + 0.15f*dz;
                        }
                    }

                    if (x0<right_most && VIDEO_3D_MONO!=video->Stereoscopic3D()) {
                        TexCoord const& tc = texcoords_[DRAW_TEXT_3D];
                        x1 = x0 + dz*tc.AspectRatio();
                        decal.x1 = x1 + 0.15f*dz;
                        if (x1>left_most) {
                            s0 = tc.x0;
                            s1 = tc.x1;
                            if (x0<left_most) {
                                s0 += (s1-s0)*(left_most-x0)/(x1-x0);
                                x0 = left_most;
                            }
                            else if (x1>right_most) {
                                s1 = s1 + (s1-s0)*(right_most-x0)/(x1-x0);
                                x1 = right_most;
                            }

                            TextRect& t = tags[tags_count++];
                            t.rt.x0 = x0;
                            t.rt.x1 = x1;
                            t.rt.z0 = z0;
                            t.rt.z1 = z1;
                            t.tc.x0 = s0;
                            t.tc.x1 = s1;
                            t.tc.y0 = tc.y0;
                            t.tc.y1 = tc.y1;

                            x0 = x1 + 0.15f*dz;
                        }
                    }
#if 0
                    int const sizeW = video->Width();
                    int const sizeH = video->Height();
                    if (x0<right_most && sizeW>=1280 && sizeH>=720) {
                        DRAW_GLYPH size = DRAW_TEXT_720P;
                        if (sizeW>=3840 && sizeH>=2160) {
                            size = (sizeW>=7680 && sizeH>=4320) ? DRAW_TEXT_8K:DRAW_TEXT_4K;
                        }
                        else if (sizeW>=1920 && sizeH>=1080) {
                            size = DRAW_TEXT_1080P;
                        }

                        TexCoord const& tc = texcoords_[size];
                        x1 = x0 + dz*tc.AspectRatio();
                        decal.x1 = x1 + 0.25f*dz;
                        if (x1>left_most) {
                            s0 = tc.x0;
                            s1 = tc.x1;
                            if (x0<left_most) {
                                s0 += (s1-s0)*(left_most-x0)/(x1-x0);
                                x0 = left_most;
                            }
                            else if (x1>right_most) {
                                s1 = s1 + (s1-s0)*(right_most-x0)/(x1-x0);
                                x1 = right_most;
                            }

                            TextRect& t = tags[tags_count++];
                            t.rt.x0 = x0;
                            t.rt.x1 = x1;
                            t.rt.z0 = z0;
                            t.rt.z1 = z1;
                            t.tc.x0 = s0;
                            t.tc.x1 = s1;
                            t.tc.y0 = tc.y0;
                            t.tc.y1 = tc.y1;

                            x0 = x1 + 0.25f*dz;
                        }
                    }
#endif
                    if (decal.x1>left_most && decal.x0<decal.x1) {
                        if (decal.x0<left_most) decal.x0 = left_most;
                        if (decal.x1>right_most) decal.x1 = right_most;
                        if (decal.x0<decal.x1) {
                            prim.AddVertex(decal.x0, y, decal.z0);
                            prim.AddVertex(decal.x0, y, decal.z1);
                            prim.AddVertex(decal.x1, y, decal.z1);
                            prim.AddVertex(decal.x1, y, decal.z0);
                        }
                    }
                }
                else {
                    break;
                }
            }
        }
    }
    prim.EndDraw();

    if (tags_count>0) {
        renderer.SetEffect(fontFx_);
        fontFx_->SetSampler(shader::semantics::diffuseMap, fonts_[0]);
        prim.BeginDraw(GFXPT_QUADLIST);
        //prim.SetColor(viveColor_);
        for (int i=0; i<tags_count; ++i) {
            TextRect& t = tags[i];
            prim.AddVertex(t.rt.x0, y, t.rt.z0, t.tc.x0, t.tc.y0);
            prim.AddVertex(t.rt.x0, y, t.rt.z1, t.tc.x0, t.tc.y1);
            prim.AddVertex(t.rt.x1, y, t.rt.z1, t.tc.x1, t.tc.y1);
            prim.AddVertex(t.rt.x1, y, t.rt.z0, t.tc.x1, t.tc.y0);
        }
        prim.EndDraw();
    }
    renderer.PopState();

    // draw 2 controller models
    renderer.SetZTest();
    for (int i=0; i<2; ++i) {
        mlabs::balai::VR::TrackedDevice const* device = vrMgr_.GetTrackedDevice(i);
        if (device)
            device->DrawSelf();
    }

    renderer.PopState();

    return (0!=quit_count_down_);
}
//---------------------------------------------------------------------------------------
bool VRVideoPlayer::DrawWidgets_(mlabs::balai::VR::HMD_EYE eye) const
{
    float const current_time = (float) mlabs::balai::system::GetTime();
    float const dashboard_x1 = 0.5f*dashboard_width_;
    float const dashboard_x0 = -dashboard_x1;
    float const dashboard_z1 = -0.5f*dashboard_height_;

    Rectangle seekRect;
    bool const show_seekbar = GetUIWidgetRect_(seekRect, DRAW_UI_SEEK_BAR);
    float dashboard_z0 = widget_top_;
    if (!show_seekbar) {
        GetUIWidgetRect_(seekRect, DRAW_UI_REPLAY); // never fail
        dashboard_z0 = 0.5f*(dashboard_z0+seekRect.z0);
    }

    float alpha = 1.0f;
    float popup = 0.0f; // y
    float y_jump = 0.0f;
    float const y_jump_max = -0.01f*dashboard_width_;
    if ((widget_timer_+animation_duration_)>current_time) {
        alpha = (current_time-widget_timer_)/animation_duration_;
        alpha *= alpha;
        if (2==widget_on_) {
            dashboard_z0 = dashboard_z1 + widget_height_*alpha;
        }
        else if (1==widget_on_) {
            dashboard_z0 -= widget_height_*alpha;
            alpha = 1.0f - alpha;
        }

        popup = 2.0f*(alpha-0.5f); // [-1, 1]
        popup = y_jump_max*(1.0f - popup*popup);
    }
    else if (1==widget_on_) {
        return true; // faded out
    }

    Renderer& renderer = Renderer::GetInstance();
    Primitives& prim = Primitives::GetInstance();

    renderer.PushState();

    // disable Z
    renderer.SetWorldMatrix(dashboard_xform_);
    renderer.SetCullDisable();

    renderer.SetDepthWrite(false);
    renderer.SetZTestDisable();

    renderer.SetBlendMode(GFXBLEND_SRCALPHA, GFXBLEND_INVSRCALPHA);
    Rectangle rect, sub_lan_rect;
    float seek_cursor_x = 0.0f;
    float seek_cursor_z = 0.0f;
    Color color = { 0, 0, 0, 64 };

    prim.BeginDraw(NULL, GFXPT_QUADLIST);
        // decal base
        prim.SetColor(color);
        prim.AddVertex(dashboard_x0, 0.0f, dashboard_z0);
        prim.AddVertex(dashboard_x0, 0.0f, dashboard_z1);
        prim.AddVertex(dashboard_x1, 0.0f, dashboard_z1);
        prim.AddVertex(dashboard_x1, 0.0f, dashboard_z0);
        if (show_seekbar && ((alpha>=1.0f && widget_on_>1) || widget_on_>2)) {
            // seek bar
            color.r = color.g = color.b = 255; color.a = 128;
            if (1==widget_on_ || 2==widget_on_) {
                color.a = (uint8) (128*alpha);
            }
            else {
                color.a = 128;
            }

            float const max_ext = 1.4f;
            rect = seekRect;
            if (4==widget_on_) {
                rect.Extend(1.0f, 1.0f+(1.0f-alpha)*(max_ext-1.0f));
            }
            else if (3==widget_on_) {
                rect.Extend(1.0f, 1.0f+alpha*(max_ext-1.0f));
            }
            else if (5==widget_on_ || 6==widget_on_) {
                rect.Extend(1.0f, max_ext);
            }
            prim.SetColor(color);
            prim.AddVertex(rect.x0, 0.0f, rect.z0);
            prim.AddVertex(rect.x0, 0.0f, rect.z1);
            prim.AddVertex(rect.x1, 0.0f, rect.z1);
            prim.AddVertex(rect.x1, 0.0f, rect.z0);

            float playing = decoder_.PlaybackProgress();
            seek_cursor_x = seekRect.x0 + (seekRect.x1-seekRect.x0)*playing;
            seek_cursor_z = 0.5f*(seekRect.z0+seekRect.z1);
            prim.SetColor((6==widget_on_||decoder_.IsPlaying()) ? viveColor_:(Color::Red));
            prim.AddVertex(rect.x0, 0.0f, rect.z0);
            prim.AddVertex(rect.x0, 0.0f, rect.z1);
            prim.AddVertex(seek_cursor_x, 0.0f, rect.z1);
            prim.AddVertex(seek_cursor_x, 0.0f, rect.z0);
        }

        //
        // bouble menu base here
        int const ext_subtitle_offset = decoder_.TotalStreamCount();
        int lan_sub_streams_start = 0;
        int lan_sub_streams_end = 0;
        int current_stream_id = -1;
        bool show_sel_cursor = false;
        DRAW_GLYPH lan_sub_type = DRAW_GLYPH_TOTALS;
        ISO_639 const* stream_languages = NULL;
        int const*     stream_indices = NULL;
        if (15==widget_on_) {
            lan_sub_streams_start = 0;
            lan_sub_streams_end = total_audio_streams_;
            lan_sub_type = DRAW_UI_LANGUAGE;
            current_stream_id = decoder_.AudioStreamID();
            stream_languages = languages_;
            stream_indices = lanStmIDs_;
            show_sel_cursor = true;
        }
        else if (16==widget_on_) {
            lan_sub_streams_start = menu_auto_scroll_;
            lan_sub_streams_end = lan_sub_streams_start + 1;
            lan_sub_type = DRAW_UI_LANGUAGE;
            current_stream_id = menu_auto_scroll_;
            stream_languages = languages_;
            stream_indices = lanStmIDs_;
        }
        else if (17==widget_on_) {
            lan_sub_streams_start = 0;
            lan_sub_streams_end = total_subtitle_streams_ + 1;
            lan_sub_type = DRAW_UI_SUBTITLE;
            current_stream_id = decoder_.SubtitleStreamID();
            stream_languages = subtitles_;
            stream_indices = subStmIDs_;
            show_sel_cursor = true;
        }
        else if (18==widget_on_) {
            lan_sub_streams_start = menu_auto_scroll_;
            lan_sub_streams_end = lan_sub_streams_start + 1;
            lan_sub_type = DRAW_UI_SUBTITLE;
            current_stream_id = menu_auto_scroll_;
            stream_languages = subtitles_;
            stream_indices = subStmIDs_;
        }

        if (lan_sub_streams_start<lan_sub_streams_end) {
            if (GetLanguageSubtitleRect_(sub_lan_rect, lan_sub_type, -1)) {
                if (show_sel_cursor) {
                    color.r = color.g = color.b = 0; color.a = 64;
                    prim.SetColor(color);
                    prim.AddVertex(sub_lan_rect.x0, 0.0f, sub_lan_rect.z0);
                    prim.AddVertex(sub_lan_rect.x0, 0.0f, sub_lan_rect.z1);
                    prim.AddVertex(sub_lan_rect.x1, 0.0f, sub_lan_rect.z1);
                    prim.AddVertex(sub_lan_rect.x1, 0.0f, sub_lan_rect.z0);
                }

                color.r = color.g = color.b = 128; color.a = 128;
                prim.SetColor(color);
                for (int i=lan_sub_streams_start; i<lan_sub_streams_end; ++i) {
                    if (GetLanguageSubtitleRect_(rect, lan_sub_type, i)) {
                        prim.AddVertex(rect.x0, 0.0f, rect.z0);
                        prim.AddVertex(rect.x0, 0.0f, rect.z1);
                        prim.AddVertex(rect.x1, 0.0f, rect.z1);
                        prim.AddVertex(rect.x1, 0.0f, rect.z0);
                    }
                }
            }
        }
    prim.EndDraw();

    //
    // bouble menu items here...
    if (lan_sub_streams_start<lan_sub_streams_end) {
        renderer.SetEffect(fontFx_);
        fontFx_->SetSampler(shader::semantics::diffuseMap, fonts_[0]);
        prim.BeginDraw(GFXPT_QUADLIST);
        bool reset_color = false;
        if (lan_sub_type==DRAW_UI_LANGUAGE) {
            for (int i=lan_sub_streams_start; i<lan_sub_streams_end; ++i) {
                if (GetLanguageSubtitleRect_(rect, lan_sub_type, i)) {
                    if (!show_sel_cursor || current_stream_id==stream_indices[i]) {
                        prim.SetColor(viveColor_);
                        reset_color = true;
                    }
                    else if (reset_color) {
                        prim.SetColor(Color::White);
                        reset_color = false;
                    }

                    //
                    // need to crop!? or early break?
                    //
                    if (i==menu_auto_scroll_) {
                        sub_lan_rect = rect;
                    }

                    if (ISO_639_UNKNOWN!=stream_languages[i]) {
                        TexCoord const& tc = texcoords_[DRAW_GLYPH_TOTALS+stream_languages[i]];
                        float const w = rect.Height() * tc.AspectRatio();
                        rect.x0 = 0.5f*(rect.x0+rect.x1-w);
                        rect.x1 = rect.x0 + w;
                        prim.AddVertex(rect.x0, 0.0f, rect.z0, tc.x0, tc.y0);
                        prim.AddVertex(rect.x0, 0.0f, rect.z1, tc.x0, tc.y1);
                        prim.AddVertex(rect.x1, 0.0f, rect.z1, tc.x1, tc.y1);
                        prim.AddVertex(rect.x1, 0.0f, rect.z0, tc.x1, tc.y0);
                    }
                    else {
                        int const id = stream_indices[i]<99 ? (0<=stream_indices[i]? stream_indices[i]:0):99;
                        TexCoord const& tc = texcoords_[DRAW_TEXT_TRACK_NO];
                        TexCoord const& tc1 = texcoords_[DRAW_TEXT_0 + (id/10)];
                        TexCoord const& tc2 = texcoords_[DRAW_TEXT_0 + (id%10)];

                        float const w1 = rect.Height() * tc.AspectRatio();
                        float const w2 = rect.Height() * tc1.AspectRatio();
                        float const w3 = rect.Height() * tc2.AspectRatio();
                        float const w = w1 + w2 + w3;

                        rect.x0 = 0.5f*(rect.x0+rect.x1-w);
                        rect.x1 = rect.x0 + w1;
                        prim.AddVertex(rect.x0, 0.0f, rect.z0, tc.x0, tc.y0);
                        prim.AddVertex(rect.x0, 0.0f, rect.z1, tc.x0, tc.y1);
                        prim.AddVertex(rect.x1, 0.0f, rect.z1, tc.x1, tc.y1);
                        prim.AddVertex(rect.x1, 0.0f, rect.z0, tc.x1, tc.y0);

                        rect.x0 = rect.x1;
                        rect.x1 = rect.x0 + w2;
                        prim.AddVertex(rect.x0, 0.0f, rect.z0, tc1.x0, tc1.y0);
                        prim.AddVertex(rect.x0, 0.0f, rect.z1, tc1.x0, tc1.y1);
                        prim.AddVertex(rect.x1, 0.0f, rect.z1, tc1.x1, tc1.y1);
                        prim.AddVertex(rect.x1, 0.0f, rect.z0, tc1.x1, tc1.y0);

                        rect.x0 = rect.x1;
                        rect.x1 = rect.x0 + w3;
                        prim.AddVertex(rect.x0, 0.0f, rect.z0, tc2.x0, tc2.y0);
                        prim.AddVertex(rect.x0, 0.0f, rect.z1, tc2.x0, tc2.y1);
                        prim.AddVertex(rect.x1, 0.0f, rect.z1, tc2.x1, tc2.y1);
                        prim.AddVertex(rect.x1, 0.0f, rect.z0, tc2.x1, tc2.y0);
                    }
                }
            }
        }
        else { // subtitle
            for (int i=lan_sub_streams_start; i<lan_sub_streams_end; ++i) {
                if (GetLanguageSubtitleRect_(rect, lan_sub_type, i)) {
                    //
                    // need to crop!? or early break?
                    //
                    if (i==menu_auto_scroll_) {
                        sub_lan_rect = rect;
                    }

                    if (total_subtitle_streams_==i) {
                        if (current_stream_id==-1) {
                            Color const clr_subtitle_disable = { 0xcc, 0x40, 0x40, 255 };
                            prim.SetColor(clr_subtitle_disable);
                        }
                        else {
                            Color const clr_subtitle_disable = { 0x20, 0x20, 0x20, 255 };
                            prim.SetColor(clr_subtitle_disable);
                        }

                        TexCoord const& tc = texcoords_[DRAW_TEXT_SUBTITLE_DISABLE];
                        float const w = rect.Height() * tc.AspectRatio();
                        rect.x0 = 0.5f*(rect.x0+rect.x1-w);
                        rect.x1 = rect.x0 + w;
                        prim.AddVertex(rect.x0, 0.0f, rect.z0, tc.x0, tc.y0);
                        prim.AddVertex(rect.x0, 0.0f, rect.z1, tc.x0, tc.y1);
                        prim.AddVertex(rect.x1, 0.0f, rect.z1, tc.x1, tc.y1);
                        prim.AddVertex(rect.x1, 0.0f, rect.z0, tc.x1, tc.y0);
                        break;
                    }

                    if (current_stream_id==stream_indices[i] || !show_sel_cursor) {
                        prim.SetColor(viveColor_);
                        reset_color = true;
                    }
                    else if (reset_color) {
                        prim.SetColor(Color::White);
                        reset_color = false;
                    }

                    if (stream_indices[i]<ext_subtitle_offset) {
                        if (ISO_639_UNKNOWN!=stream_languages[i]) {
                            TexCoord const& tc = texcoords_[DRAW_GLYPH_TOTALS+stream_languages[i]];
                            float const w = rect.Height() * tc.AspectRatio();
                            rect.x0 = 0.5f*(rect.x0+rect.x1-w);
                            rect.x1 = rect.x0 + w;
                            prim.AddVertex(rect.x0, 0.0f, rect.z0, tc.x0, tc.y0);
                            prim.AddVertex(rect.x0, 0.0f, rect.z1, tc.x0, tc.y1);
                            prim.AddVertex(rect.x1, 0.0f, rect.z1, tc.x1, tc.y1);
                            prim.AddVertex(rect.x1, 0.0f, rect.z0, tc.x1, tc.y0);
                        }
                        else {
                            int const id = stream_indices[i]<99 ? (0<=stream_indices[i]? stream_indices[i]:0):99;
                            TexCoord const& tc = texcoords_[DRAW_TEXT_TRACK_NO];
                            TexCoord const& tc1 = texcoords_[DRAW_TEXT_0 + (id/10)];
                            TexCoord const& tc2 = texcoords_[DRAW_TEXT_0 + (id%10)];

                            float const w1 = rect.Height() * tc.AspectRatio();
                            float const w2 = rect.Height() * tc1.AspectRatio();
                            float const w3 = rect.Height() * tc2.AspectRatio();
                            float const w = w1 + w2 + w3;

                            rect.x0 = 0.5f*(rect.x0+rect.x1-w);
                            rect.x1 = rect.x0 + w1;
                            prim.AddVertex(rect.x0, 0.0f, rect.z0, tc.x0, tc.y0);
                            prim.AddVertex(rect.x0, 0.0f, rect.z1, tc.x0, tc.y1);
                            prim.AddVertex(rect.x1, 0.0f, rect.z1, tc.x1, tc.y1);
                            prim.AddVertex(rect.x1, 0.0f, rect.z0, tc.x1, tc.y0);

                            rect.x0 = rect.x1;
                            rect.x1 = rect.x0 + w2;
                            prim.AddVertex(rect.x0, 0.0f, rect.z0, tc1.x0, tc1.y0);
                            prim.AddVertex(rect.x0, 0.0f, rect.z1, tc1.x0, tc1.y1);
                            prim.AddVertex(rect.x1, 0.0f, rect.z1, tc1.x1, tc1.y1);
                            prim.AddVertex(rect.x1, 0.0f, rect.z0, tc1.x1, tc1.y0);

                            rect.x0 = rect.x1;
                            rect.x1 = rect.x0 + w3;
                            prim.AddVertex(rect.x0, 0.0f, rect.z0, tc2.x0, tc2.y0);
                            prim.AddVertex(rect.x0, 0.0f, rect.z1, tc2.x0, tc2.y1);
                            prim.AddVertex(rect.x1, 0.0f, rect.z1, tc2.x1, tc2.y1);
                            prim.AddVertex(rect.x1, 0.0f, rect.z0, tc2.x1, tc2.y0);
                        }
                    }
                }
            }
        }
        prim.EndDraw();

        if (lan_sub_type==DRAW_UI_SUBTITLE && decoder_.ExternalSubtitleStreamCount()>0) {
            fontFx_->SetSampler(shader::semantics::diffuseMap, extSubtitleInfo_);
            prim.BeginDraw(GFXPT_QUADLIST);
            reset_color = false;

            float const tex_aspect_ratio = (float) extSubtitleInfo_->Width()/(float)extSubtitleInfo_->Height();
            for (int i=lan_sub_streams_start; i<lan_sub_streams_end; ++i) {
                if (total_subtitle_streams_==i)
                    break;

                if (GetLanguageSubtitleRect_(rect, lan_sub_type, i)) {
                    TexCoord const& tc = extSubtitleFilename_[i];

                    if (current_stream_id==stream_indices[i] || !show_sel_cursor) {
                        prim.SetColor(viveColor_);
                        reset_color = true;
                    }
                    else if (reset_color) {
                        prim.SetColor(Color::White);
                        reset_color = false;
                    }

                    if (stream_indices[i]>=ext_subtitle_offset) {
                        float const w = rect.Height() * tc.AspectRatio() * tex_aspect_ratio;
                        rect.x0 = 0.5f*(rect.x0+rect.x1-w);
                        rect.x1 = rect.x0 + w;

                        prim.AddVertex(rect.x0, 0.0f, rect.z0, tc.x0, tc.y0);
                        prim.AddVertex(rect.x0, 0.0f, rect.z1, tc.x0, tc.y1);
                        prim.AddVertex(rect.x1, 0.0f, rect.z1, tc.x1, tc.y1);
                        prim.AddVertex(rect.x1, 0.0f, rect.z0, tc.x1, tc.y0);
                    }
                }
            }

            prim.EndDraw();
        }

        if (show_sel_cursor && menu_auto_scroll_<lan_sub_streams_end) {
            prim.BeginDraw(NULL, GFXPT_LINESTRIP);
            prim.SetColor(viveColor_);
            prim.AddVertex(sub_lan_rect.x0, 0.0f, sub_lan_rect.z0);
            prim.AddVertex(sub_lan_rect.x0, 0.0f, sub_lan_rect.z1);
            prim.AddVertex(sub_lan_rect.x1, 0.0f, sub_lan_rect.z1);
            prim.AddVertex(sub_lan_rect.x1, 0.0f, sub_lan_rect.z0);
            prim.AddVertex(sub_lan_rect.x0, 0.0f, sub_lan_rect.z0);
            prim.EndDraw();
        }
    }

    if (((alpha>=1.0f && widget_on_>1) || widget_on_>2)) {
        Rectangle rect_ref;
        prim.BeginDraw(uiGlyph_, GFXPT_QUADLIST);

        // previous - replay
        if (GetUIWidgetRect_(rect, DRAW_UI_REPLAY)) {
            TexCoord const& tc = texcoords_[DRAW_UI_REPLAY];
            y_jump = (7==widget_on_) ? popup:0.0f;
            prim.AddVertex(rect.x0, y_jump, rect.z0, tc.x0, tc.y0);
            prim.AddVertex(rect.x0, y_jump, rect.z1, tc.x0, tc.y1);
            prim.AddVertex(rect.x1, y_jump, rect.z1, tc.x1, tc.y1);
            prim.AddVertex(rect.x1, y_jump, rect.z0, tc.x1, tc.y0);
        }

        // play/pause
        GetUIWidgetRect_(rect, DRAW_UI_PAUSE); // = DRAW_UI_PLAY
        rect_ref = rect;
        if (decoder_.IsPlaying()) {
            // pause to play
            TexCoord const& tc = texcoords_[DRAW_UI_PAUSE];
            if (9==widget_on_ && alpha<1.0f) {
                prim.AddVertex(rect.x0, 0.0f, rect.z0, tc.x0, tc.y0);
                prim.AddVertex(rect.x0, 0.0f, rect.z1, tc.x0, tc.y1);
                float d = 0.5f*(1.0f-alpha)*(rect.z0-rect.z1);
                float y0 = rect.z0 - d;
                float y1 = rect.z1 + d;

                d = 0.5f*(1.0f-alpha)*(tc.y1-tc.y0);
                float t0 = tc.y0 + d;
                float t1 = tc.y1 - d;
                prim.AddVertex(rect.x1, 0.0f, y1, tc.x1, t1);
                prim.AddVertex(rect.x1, 0.0f, y0, tc.x1, t0);
            }
            else {
                y_jump = (8==widget_on_) ? popup:0.0f;
                prim.AddVertex(rect.x0, y_jump, rect.z0, tc.x0, tc.y0);
                prim.AddVertex(rect.x0, y_jump, rect.z1, tc.x0, tc.y1);
                prim.AddVertex(rect.x1, y_jump, rect.z1, tc.x1, tc.y1);
                prim.AddVertex(rect.x1, y_jump, rect.z0, tc.x1, tc.y0);
            }
        }
        else {
            // play to pause
            if (9==widget_on_ && alpha<1.0f) {
                TexCoord const& tc = texcoords_[DRAW_UI_PAUSE];
                prim.AddVertex(rect.x0, 0.0f, rect.z0, tc.x0, tc.y0);
                prim.AddVertex(rect.x0, 0.0f, rect.z1, tc.x0, tc.y1);

                float d = 0.5f*alpha*(rect.z0-rect.z1);
                float y0 = rect.z0 - d;
                float y1 = rect.z1 + d;

                d = 0.5f*alpha*(tc.y1-tc.y0);
                float t0 = tc.y0 + d;
                float t1 = tc.y1 - d;
                prim.AddVertex(rect.x1, 0.0f, y1, tc.x1, t1);
                prim.AddVertex(rect.x1, 0.0f, y0, tc.x1, t0);
            }
            else {
                TexCoord const& tc = texcoords_[DRAW_UI_PLAY];
                y_jump = (8==widget_on_) ? popup:0.0f;
                prim.AddVertex(rect.x0, y_jump, rect.z0, tc.x0, tc.y0);
                prim.AddVertex(rect.x0, y_jump, rect.z1, tc.x0, tc.y1);
                prim.AddVertex(rect.x1, y_jump, rect.z1, tc.x1, tc.y1);
                prim.AddVertex(rect.x1, y_jump, rect.z0, tc.x1, tc.y0);
            }
        }

        // next track
        int const totals = playList_.size();
        bool show_next_track = false;
        bool show_volume_bar = false;
        float master_volume = 0.0f;
        float timestamp_left = rect.x1;
        Rectangle rectNextTrack;
        if (totals>1 && GetUIWidgetRect_(rectNextTrack, DRAW_UI_NEXT)) {
            TexCoord const& tc = texcoords_[DRAW_UI_NEXT];
            y_jump = (10==widget_on_) ? popup:0.0f;
            prim.AddVertex(rectNextTrack.x0, y_jump, rectNextTrack.z0, tc.x0, tc.y0);
            prim.AddVertex(rectNextTrack.x0, y_jump, rectNextTrack.z1, tc.x0, tc.y1);
            prim.AddVertex(rectNextTrack.x1, y_jump, rectNextTrack.z1, tc.x1, tc.y1);
            prim.AddVertex(rectNextTrack.x1, y_jump, rectNextTrack.z0, tc.x1, tc.y0);
            timestamp_left = rectNextTrack.x1;
        }

        // volume
        if (masterVolume_ && GetUIWidgetRect_(rect, DRAW_UI_VOLUME)) {
            master_volume = masterVolume_.GetVolume(true);
            TexCoord const& tc = texcoords_[master_volume>0.0f ? DRAW_UI_VOLUME:DRAW_UI_MUTE];
            y_jump = (11==widget_on_) ? popup:0.0f;
            prim.AddVertex(rect.x0, y_jump, rect.z0, tc.x0, tc.y0);
            prim.AddVertex(rect.x0, y_jump, rect.z1, tc.x0, tc.y1);
            prim.AddVertex(rect.x1, y_jump, rect.z1, tc.x1, tc.y1);
            prim.AddVertex(rect.x1, y_jump, rect.z0, tc.x1, tc.y0);
            show_volume_bar = (12==widget_on_) || (11==widget_on_);
            timestamp_left = rect.x1;
        }

        // back to menu
        if (GetUIWidgetRect_(rect, DRAW_UI_MENU)) {
            TexCoord const& tc = texcoords_[DRAW_UI_MENU];
            y_jump = (14==widget_on_) ? popup:0.0f;
            prim.AddVertex(rect.x0, y_jump, rect.z0, tc.x0, tc.y0);
            prim.AddVertex(rect.x0, y_jump, rect.z1, tc.x0, tc.y1);
            prim.AddVertex(rect.x1, y_jump, rect.z1, tc.x1, tc.y1);
            prim.AddVertex(rect.x1, y_jump, rect.z0, tc.x1, tc.y0);
        }

        if (GetUIWidgetRect_(rect, DRAW_UI_LIGHT)) {
            TexCoord const& tc = texcoords_[DRAW_UI_LIGHT];
            uint8 const dif[DIM_BACKGROUND_CONTROLS] = {
                192, 92, 32, 255
            };

            color.r = dif[dim_bkg_%DIM_BACKGROUND_CONTROLS];
            if (dim_bkg_!=2) {
                color.g = color.b = color.r;
            }
            else {
                color.g = color.b = 0;
            }
            color.a = 255;
            y_jump = (13==widget_on_) ? popup:0.0f;
            prim.SetColor(color);
            prim.AddVertex(rect.x0, y_jump, rect.z0, tc.x0, tc.y0);
            prim.AddVertex(rect.x0, y_jump, rect.z1, tc.x0, tc.y1);
            prim.AddVertex(rect.x1, y_jump, rect.z1, tc.x1, tc.y1);
            prim.AddVertex(rect.x1, y_jump, rect.z0, tc.x1, tc.y0);
            prim.SetColor(Color::White);
        }

        // if audio/language tweakable
        if (GetUIWidgetRect_(rect, DRAW_UI_LANGUAGE)) {
            TexCoord const& tc = texcoords_[DRAW_UI_LANGUAGE];
            y_jump = (15==widget_on_) ? popup:0.0f;
            prim.AddVertex(rect.x0, y_jump, rect.z0, tc.x0, tc.y0);
            prim.AddVertex(rect.x0, y_jump, rect.z1, tc.x0, tc.y1);
            prim.AddVertex(rect.x1, y_jump, rect.z1, tc.x1, tc.y1);
            prim.AddVertex(rect.x1, y_jump, rect.z0, tc.x1, tc.y0);
        }

        // if subtitle tweakable
        if (GetUIWidgetRect_(rect, DRAW_UI_SUBTITLE)) {
            TexCoord const& tc = texcoords_[DRAW_UI_SUBTITLE];
            if (-1==decoder_.SubtitleStreamID()) {
                color.r = color.g = color.b = 192; color.a = 128;
                prim.SetColor(color);
            }
            y_jump = (17==widget_on_) ? popup:0.0f;
            prim.AddVertex(rect.x0, y_jump, rect.z0, tc.x0, tc.y0);
            prim.AddVertex(rect.x0, y_jump, rect.z1, tc.x0, tc.y1);
            prim.AddVertex(rect.x1, y_jump, rect.z1, tc.x1, tc.y1);
            prim.AddVertex(rect.x1, y_jump, rect.z0, tc.x1, tc.y0);
            prim.SetColor(Color::White);
        }

        bool show_connected_line = false;
        float cursor_x(seek_cursor_x), cursor_z(seek_cursor_z);
        if (NULL!=focus_controller_ && focus_controller_->IsTracked()) {
            TexCoord const& tc = texcoords_[DRAW_UI_CIRCLE];
            float const sh = 0.5f*(rect_ref.z0 - rect_ref.z1);
            float const sw = sh*tc.AspectRatio();
            float x0, x1, z0, z1;

            // seekbar cursor
            if (show_seekbar && (3==widget_on_ || 4==widget_on_ || 5==widget_on_ || 6==widget_on_)) {
                float s0 = seekRect.Height();
                float s1 = 1.2f*sw;
                float s  = s1;
                if (4==widget_on_) {
                    s = alpha*s0 + (1.0f-alpha)*s1;
                }
                else if (3==widget_on_) {
                    s = (1.0f-alpha)*s0 + alpha*s1;
                }
                x0 = seek_cursor_x - 0.5f*s;
                x1 = x0 + s;
                z0 = seek_cursor_z + 0.5f*s;
                z1 = z0 - s;
                prim.SetColor((6==widget_on_||decoder_.IsPlaying()) ? viveColor_:Color::Red);
                prim.AddVertex(x0, 0.0f, z0, tc.x0, tc.y0);
                prim.AddVertex(x0, 0.0f, z1, tc.x0, tc.y1);
                prim.AddVertex(x1, 0.0f, z1, tc.x1, tc.y1);
                prim.AddVertex(x1, 0.0f, z0, tc.x1, tc.y0);
            }

            if (0.0f<DashboardPickTest_(cursor_x, cursor_z, focus_controller_) &&
                (cursor_z<widget_top_ || 5==widget_on_ || 6==widget_on_)) {
                if (totals>1 && 0==on_processing_) {
                    show_next_track = rectNextTrack.In(cursor_x, cursor_z);
                }

                if (5==widget_on_ || 6==widget_on_) {
                    //show_connected_line = true;
                }

                x0 = cursor_x - 0.5f*sw;
                x1 = x0 + sw;
                z0 = cursor_z + 0.5f*sh;
                z1 = z0 - sh;
                prim.SetColor(viveColor_);
                prim.AddVertex(x0, 0.0f, z0, tc.x0, tc.y0);
                prim.AddVertex(x0, 0.0f, z1, tc.x0, tc.y1);
                prim.AddVertex(x1, 0.0f, z1, tc.x1, tc.y1);
                prim.AddVertex(x1, 0.0f, z0, tc.x1, tc.y0);
            }
        }

        prim.EndDraw();

        if (show_volume_bar && GetUIWidgetRect_(rect, DRAW_UI_VOLUME_BAR)) {
            prim.BeginDraw(NULL, GFXPT_QUADLIST);
            float h = 0.25f * (rect.z0 - rect.z1);
            float z0 = 0.5f * (rect.z0 + rect.z1) + 0.5f*h;
            float z1 = z0 - h;

            float right_clamp = rect.x1;
            if (12==widget_on_) {
                right_clamp = alpha*rect.x0 + (1.0f-alpha)*rect.x1;
            }
            else if (11==widget_on_) {
                right_clamp = (1.0f-alpha)*rect.x0 + alpha*rect.x1;
            }
            // decal
            color = Color::White; color.a = 64;
            prim.SetColor(color);
            prim.AddVertex(rect.x0, 0.0f, z0);
            prim.AddVertex(rect.x0, 0.0f, z1);
            prim.AddVertex(right_clamp, 0.0f, z1);
            prim.AddVertex(right_clamp, 0.0f, z0);

            // volume meter
            float const vol = rect.x0 + master_volume*(rect.x1-rect.x0);
            float x0 = (vol>right_clamp) ? right_clamp:vol;
            prim.SetColor(Color::Red);
            prim.AddVertex(rect.x0, 0.0f, z0);
            prim.AddVertex(rect.x0, 0.0f, z1);
            prim.AddVertex(x0, 0.0f, z1);
            prim.AddVertex(x0, 0.0f, z0);

            // vertical toggle bar
            float w = 0.075f * (rect.x1-rect.x0);
            x0 = vol - 0.5f*w;
            right_clamp += 0.5f*w;
            if (x0<right_clamp) {
                h = 0.75f * (rect.z0 - rect.z1);
                z0 = 0.5f * (rect.z0 + rect.z1) + 0.5f*h;
                z1 = z0 - h;
                float x1 = x0 + w;
                if (x1 > right_clamp)
                    x1 = right_clamp;
                prim.SetColor(Color::White);
                prim.AddVertex(x0, 0.0f, z0);
                prim.AddVertex(x0, 0.0f, z1);
                prim.AddVertex(x1, 0.0f, z1);
                prim.AddVertex(x1, 0.0f, z0);
            }
            timestamp_left = right_clamp;
            prim.EndDraw();
        }

        //
        // show timestamp here  01:23:45 / 76:54:32
        // (timestamp_left)
        float const digit_height = 1.5f*rect_ref.Height();
        float const z0 = 0.5f*(rect_ref.z0+rect_ref.z1 + 0.95f*digit_height);
        float const z1 = z0 - digit_height;
        float const hh = 0.6f*digit_height; //0.75f*rect_ref.Height();
        float const unit = hh*texcoords_[DRAW_TEXT_0].AspectRatio();
        int glyphs[17];
        float lefts[17];
        int glyph_totals = 0;
        lefts[0] = timestamp_left += 0.115f;// timestamp_left += 0.25f*hh;
        int const ds = decoder_.GetDuration()/1000;
        if (ds>3600) { // >= 1 hr
            glyph_totals = 17;
            int ss = decoder_.Timestamp()/1000; if (ss<0) ss = 0;
            int hour = ss/3600; ss %= 3600; if (hour>99) hour = 99;
            int mm = ss/60;  ss %= 60;
            glyphs[0] = DRAW_TEXT_0 + (hour/10); lefts[1] = lefts[0] + hh*texcoords_[glyphs[0]].AspectRatio();
            glyphs[1] = DRAW_TEXT_0 + (hour%10); lefts[2] = timestamp_left + 2.0f*unit;
            glyphs[2] = DRAW_TEXT_COLON;       lefts[3] = lefts[2] + 0.5f*unit;
            glyphs[3] = DRAW_TEXT_0 + (mm/10); lefts[4] = lefts[3] + hh*texcoords_[glyphs[3]].AspectRatio();
            glyphs[4] = DRAW_TEXT_0 + (mm%10); lefts[5] = lefts[3] + 2.0f*unit;
            glyphs[5] = DRAW_TEXT_COLON;       lefts[6] = lefts[5] + 0.5f*unit;
            glyphs[6] = DRAW_TEXT_0 + (ss/10); lefts[7] = lefts[6] + hh*texcoords_[glyphs[6]].AspectRatio();
            glyphs[7] = DRAW_TEXT_0 + (ss%10); lefts[8] = lefts[7] + hh*texcoords_[glyphs[7]].AspectRatio();

            timestamp_left += 7.8f*unit;
            glyphs[8] = DRAW_TEXT_SLASH; lefts[9] = timestamp_left;

            ss = ds;
            hour = ss/3600; ss %= 3600; if (hour>99) hour = 99;
            mm = ss/60;  ss %= 60;
            glyphs[9] = DRAW_TEXT_0 + (hour/10);  lefts[10] = lefts[9] + hh*texcoords_[glyphs[9]].AspectRatio();
            glyphs[10] = DRAW_TEXT_0 + (hour%10); lefts[11] = timestamp_left + 2.0f*unit;
            glyphs[11] = DRAW_TEXT_COLON;       lefts[12] = lefts[11] + 0.5f*unit;
            glyphs[12] = DRAW_TEXT_0 + (mm/10); lefts[13] = lefts[12] + hh*texcoords_[glyphs[12]].AspectRatio();
            glyphs[13] = DRAW_TEXT_0 + (mm%10); lefts[14] = lefts[12] + 2.0f*unit;
            glyphs[14] = DRAW_TEXT_COLON;       lefts[15] = lefts[14] + 0.5f*unit;
            glyphs[15] = DRAW_TEXT_0 + (ss/10); lefts[16] = lefts[15] + hh*texcoords_[glyphs[15]].AspectRatio();
            glyphs[16] = DRAW_TEXT_0 + (ss%10);
        }
        else {
            int ss = decoder_.Timestamp()/1000; if (ss<0) ss = 0;
            int mm = ss/60;
            if (ss>0) {
                ss %= 60;
                glyphs[0] = DRAW_TEXT_0 + (mm/10); lefts[1] = lefts[0] + hh*texcoords_[glyphs[0]].AspectRatio();
                glyphs[1] = DRAW_TEXT_0 + (mm%10); lefts[2] = timestamp_left + 2.0f*unit;
                glyphs[2] = DRAW_TEXT_COLON;       lefts[3] = lefts[2] + 0.5f*unit;
                glyphs[3] = DRAW_TEXT_0 + (ss/10); lefts[4] = lefts[3] + hh*texcoords_[glyphs[3]].AspectRatio();
                glyphs[4] = DRAW_TEXT_0 + (ss%10); lefts[5] = lefts[3] + 2.0f*unit;
                glyph_totals = 5;

                if (ds>0) {
                    timestamp_left += 5.35f*unit;
                    glyphs[5] = DRAW_TEXT_SLASH; lefts[6] = timestamp_left;

                    ss = ds;
                    mm = ss/60; ss %= 60;
                    glyphs[6] = DRAW_TEXT_0 + (mm/10);  lefts[7] = lefts[6] + hh*texcoords_[glyphs[6]].AspectRatio();
                    glyphs[7] = DRAW_TEXT_0 + (mm%10);  lefts[8] = timestamp_left + 2.0f*unit;
                    glyphs[8] = DRAW_TEXT_COLON;        lefts[9] = lefts[8] + 0.5f*unit;
                    glyphs[9] = DRAW_TEXT_0 + (ss/10);  lefts[10] = lefts[9] + hh*texcoords_[glyphs[9]].AspectRatio();
                    glyphs[10] = DRAW_TEXT_0 + (ss%10);
                    glyph_totals = 11;
                }
            }
        }

        renderer.SetEffect(fontFx_);
        fontFx_->SetSampler(shader::semantics::diffuseMap, fonts_[0]);
        prim.BeginDraw(GFXPT_QUADLIST);
        for (int i=0; i<glyph_totals; ++i) {
            TexCoord const& tc1 = texcoords_[glyphs[i]];
            float x0 = lefts[i];
            float x1 = x0 + hh * tc1.AspectRatio();
            prim.AddVertex(x0, 0.0f, z0, tc1.x0, tc1.y0);
            prim.AddVertex(x0, 0.0f, z1, tc1.x0, tc1.y1);
            prim.AddVertex(x1, 0.0f, z1, tc1.x1, tc1.y1);
            prim.AddVertex(x1, 0.0f, z0, tc1.x1, tc1.y0);
        }
        prim.EndDraw();

        // a line connects pointer cursor and seek cursor in seek bar
        if (show_connected_line) {
            prim.BeginDraw(NULL, GFXPT_LINELIST);
            color = viveColor_; color.a = 128;
            prim.SetColor(color);
            prim.AddVertex(cursor_x, 0.0f, cursor_z);
            prim.AddVertex(seek_cursor_x, 0.0f, seek_cursor_z);
            prim.EndDraw();
        }

        // this renders after cursor point, make sure the 2 never overlap.
        if (show_next_track) {
            VideoTrack* next = NULL;
            VideoTrack* prev = NULL;
            for (int i=0; i<totals; ++i) {
                if (playList_[i]==current_track_) {
                    next = playList_[(i+1)%totals];
                    prev = playList_[(i+totals-1)%totals];
                }
            }

            if (NULL!=next) {
                int tid = next->ThumbnailTextureId();
                ITexture* tex = (0<=tid && tid<MENU_VIDEO_THUMBNAIL_TEXTURES) ? videoThumbnails_[tid]:NULL;
                if (NULL!=tex) {
                    prim.BeginDraw(tex, GFXPT_QUADLIST);
                    float const sh = 4.0f*(rectNextTrack.z0 - rectNextTrack.z1);
                    float const sw = sh*(next->IsSpherical() ? (16.0f/9.0f):next->AspectRatio());
                    int const index = ((int) floor(current_time))%4;
                    float s0 = 0.5f*(index%2); float s1 = s0 + 0.5f;
                    float t0 = 0.5f*(index/2); float t1 = t0 + 0.5f;
                    float bb = rectNextTrack.z0 + 0.05f*sh;
                    float tt = bb + sh;
                    float x0 = 0.5f*(rectNextTrack.x0 + rectNextTrack.x1) - 0.5f*sw;
                    float x1 = x0 + sw;

                    int longi(0), lat_inf(0), lati_sup(0);
                    VIDEO_3D const s3D = next->Stereoscopic3D();
                    if (VIDEO_3D_MONO!=s3D) {
                        if (0==(s3D&VIDEO_3D_TOPBOTTOM)) { // SBS
                            if ((mlabs::balai::VR::HMD_EYE_LEFT==eye) ^ (VIDEO_3D_LEFTRIGHT==s3D)) {
                                s0 = 0.5f*(s0+s1);
                            }
                            else {
                                s1 = 0.5f*(s0+s1);
                            }
                        }
                        else { // TB
                            if ((mlabs::balai::VR::HMD_EYE_LEFT==eye) ^ (VIDEO_3D_TOPBOTTOM==s3D)) {
                                t0 = 0.5f*(t0+t1);
                            }
                            else {
                                t1 = 0.5f*(t0+t1);
                            }
                        }

                        if (next->GetSphericalAngles(longi, lat_inf, lati_sup)) {
                            // oh boy! it's non-sense, actually:-)
                            float const crop_factor = (longi>180) ? 0.66f:0.8f;
                            s0 = 0.5f*(s0+s1);
                            s1 = (s1-s0)*crop_factor;
                            s0 -= s1;
                            s1 = s0 + 2.0f*s1;

                            t0 = 0.5f*(t0+t1);
                            t1 = (t1-t0)*crop_factor;
                            t0 -= t1;
                            t1 = t0 + 2.0f*t1;
                        }
                    }
                    prim.AddVertex(x0, 0.0f, tt, s0, t0);
                    prim.AddVertex(x0, 0.0f, bb, s0, t1);
                    prim.AddVertex(x1, 0.0f, bb, s1, t1);
                    prim.AddVertex(x1, 0.0f, tt, s1, t0);
                    prim.EndDraw();
                }
/*
                // file name
                renderer.SetEffect(fontFx_);
                fontFx_->SetSampler(shader::semantics::diffuseMap, fonts_[0]);
                //renderer.CommitChanges();
                prim.BeginDraw(GFXPT_QUADLIST);

                TexCoord const& tc = texcoords_[DRAW_TEXT_NEXT];
                float th = 0.1f*sh;
                x0 += 0.1f*th; tt += 0.1f*th;
                x1 = x0 + th*tc.AspectRatio();
                bb = z0 - th;
                prim.AddVertex(x0, 0.0f, tt, tc.x0, tc.y0);
                prim.AddVertex(x0, 0.0f, bb, tc.x0, tc.y1);
                prim.AddVertex(x1, 0.0f, bb, tc.x1, tc.y1);
                prim.AddVertex(x1, 0.0f, tt, tc.x1, tc.y0);

                x0 = x1;// + 0.1f*th;

                prim.EndDraw();
*/
            }
        }
    }

    renderer.PopState();

    return true;
}

}