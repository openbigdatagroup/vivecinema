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
 * @file    Subtitle.h
 * @author  andre chen
 * @history 2016/08/25 created
 *
 */
#ifndef EXT_SUBTITLE_H
#define EXT_SUBTITLE_H

#include "SystemFont.h"
#include "BLColor.h"

#include <mutex>

using mlabs::balai::graphics::Color;

namespace mlabs { namespace balai { namespace video {

// each SubtitleRect contains only 1 line!
struct SubtitleRect {
    // andre : forgive me... i wouldn't normally do this kind of ugly classes.
    // the main concern here is to avid memory alloc/free in realtime.
    enum { ANIMATION_BUFFER_SIZE = 16 };
    struct sAnimationKey {
        int Time, Value;
        float Accl;
        sAnimationKey():Time(0),Value(0),Accl(1.0f) {}
        sAnimationKey(int t, int v):Time(t),Value(v),Accl(1.0f) {}
    };

private:
    class AnimationSet {
        sAnimationKey animationBufferKeyBuffer_[ANIMATION_BUFFER_SIZE]; 
        struct sAnimation {
            int Begin, End;
            sAnimation():Begin(0),End(0) {}
            void Reset() { Begin = End = 0; }
        };
        sAnimation fade_;   // \fade
        sAnimation moveX_, moveY_;
        sAnimation size_;   // \t([t1,t2,][accl,],\fs), float type, scale 2000x
//        sAnimation scaleX_; // \t([t1,t2,][accl,],\fscx)
//        sAnimation scaleY_; // \t([t1,t2,][accl,],\fscy)
        sAnimation roll_;   // \t([t1,t2,][accl,],\fr)
        int numKeys_;

        bool SetInternal_(sAnimation& anim, sAnimationKey const* keys, int num) {
            if (NULL!=keys && (numKeys_+num)<=ANIMATION_BUFFER_SIZE) {
                memcpy(animationBufferKeyBuffer_+numKeys_, keys, num*sizeof(sAnimationKey));
                anim.Begin = numKeys_;
                anim.End = numKeys_ += num;
                return true;
            }
            return false;
        }

        bool GetInternal_(sAnimation const& anim, int& v, int t) const {
            if (anim.Begin<anim.End) {
                int time = -1;
                for (int i=anim.Begin; i<anim.End; ++i) {
                    sAnimationKey const& a = animationBufferKeyBuffer_[i];
                    if (t<a.Time) {
                        if (i==anim.Begin) {
                            v = a.Value;
                        }
                        else {
                            float alpha = float(t-time)/float(a.Time-time);
                            if (a.Accl!=1.0f) {
                                alpha = pow(alpha, a.Accl);
                            }
                            v = (int) (v + alpha*(a.Value-v));
                        }
                        break;
                    }
                    time = a.Time;
                    v = a.Value;
                }
                return true;
            }
            return false;
        }

    public:
        AnimationSet():numKeys_(0) {}
        void Reset() {
            fade_.Reset();
            size_.Reset();
            moveX_.Reset();
            moveY_.Reset();
//            scaleX_.Reset();
//            scaleY_.Reset();
            roll_.Reset();
            numKeys_ = 0;
        }

        // set animations
        bool SetFadeAnimation(sAnimationKey const* keys, int num) {
            return 4==num && SetInternal_(fade_, keys, num);
        }
        bool SetSizeAnimation(sAnimationKey const* keys, int num) {
            return num>0 && SetInternal_(size_, keys, num);
        }/*
        bool SetScaleXAnimation(sAnimationKey const* keys, int num) {
            return num>0 && SetInternal_(scaleX_, keys, num);
        }
        bool SetScaleYAnimation(sAnimationKey const* keys, int num) {
            return num>0 && SetInternal_(scaleY_, keys, num);
        }*/
        bool SetRollAnimation(sAnimationKey const* keys, int num) {
            return num>0 && SetInternal_(roll_, keys, num);
        }
        bool SetMoveXAnimation(sAnimationKey const* keys, int num) {
            return 2==num && SetInternal_(moveX_, keys, 2);
        }
        bool SetMoveYAnimation(sAnimationKey const* keys, int num) {
            return 2==num && SetInternal_(moveY_, keys, 2);
        }

        // get animation value
        int FadeAlpha(int t) const {
            int v = 255;
            if ((fade_.Begin+4)==fade_.End && GetInternal_(fade_, v, t)) {
                return v;
            }
            return 255;
        }

        bool GetScale(float& sx, float& sy, int t) const {
            int v(2000);
/*          
            int vx(2000), vy(2000);
            bool ret = false;
            if (!GetInternal_(scaleX_, vx, t)) {
                vx = 2000;
            }

            if (!GetInternal_(scaleY_, vy, t)) {
                vx = 2000;
            }
*/
            if (GetInternal_(size_, v, t)) {
                sx = sy = 0.0005f*v;
                return true;
            }
            sx = sy = 1.0f;
            return false;
        }

        bool GetRotation(float& roll, int t) const {
            int v = 0;
            if (GetInternal_(roll_, v, t)) {
                roll = -(0.01f*v)*0.017453f;
                return true;
            }
            roll = 0.0f;
            return false;
        }

        bool GetMove(float& dx, float& dy, int t) const {
            dx = dy = 0.0f;
            int v = 0;
            if (GetInternal_(moveX_, v, t)) {
                dx = 0.01f * v;
            }
            if (GetInternal_(moveY_, v, t)) {
                dy = 0.01f * v;
            }
            return dx!=0.0f || dy!=0.0f;
        }
#if 0
        bool GetMoveOffset(float& dx, float& dy, int& t1, int& t2) const {
            if ((moveX_.Begin+2)==moveX_.End) {
                t1 = animationBufferKeyBuffer_[moveX_.Begin].Time;
                t2 = animationBufferKeyBuffer_[moveX_.Begin+1].Time;
                dx = 0.01f*(animationBufferKeyBuffer_[moveX_.Begin+1].Value);
                if ((moveY_.Begin+2)==moveY_.End) {
                    dy = 0.01f*(animationBufferKeyBuffer_[moveY_.Begin+1].Value);
                }
                else {
                    dy = 0.0f;
                }
                return t1<t2;
            }
            else if ((moveY_.Begin+2)==moveY_.End) {
                t1 = animationBufferKeyBuffer_[moveY_.Begin].Time;
                t2 = animationBufferKeyBuffer_[moveY_.Begin+1].Time;
                dx = 0.0f;
                dy = 0.01f*(animationBufferKeyBuffer_[moveY_.Begin+1].Value);
            
                return t1<t2;
            }
            return false;
        }
#endif
        bool IsMoving() const {
            return moveX_.Begin<moveX_.End ||
                   moveY_.Begin<moveY_.End ||
                   size_.Begin<size_.End || // \fs
                   roll_.Begin<roll_.End;   // \fr
                   // more!
        }

    } animationSet_;

public:
    // colors
    Color PrimaryColour, SecondaryColour, OutlineColour, BackColour;

    // timing - ms
    int TimeStart, TimeEnd; // not for hardsub
    int TimeKaraokeIn, TimeKaraokeOut;

    // ASS 4.0+ Level, will use negative value for depth
    int Layer; //

    // play resolution
    float PlayResX, PlayResY;

    // (initial) location and size
    float X, Y, Width, Height;

    /////////////////////////////////////////////////////////////////////////////////////
    // alignment pinpoint 
    // note : this only works for sizing animation of single paragraph(rect)
    //        if the dialogue consists of several paragraphs with sizing animation,
    //        the pinpoint will change with respect to all paragraphs size.
    float Xpin, Ypin;
    /////////////////////////////////////////////////////////////////////////////////////

    // rotation - origin + angles
    float Xo, Yo, Yaw, Pitch, Roll;

    // scale
    float ScaleX, ScaleY;

    // texture coordinate
    float s0, t0, s1, t1;

    uint8 Shadow;    // [0, 4]
    uint8 Border;    // [0, 4]
    uint8 Alignment; // [1, 9] numpad
    uint8 Karaoke;   // 1:\k highlight by word, 2:\kf fill up mode, 3:\ko outline highlighting

    uint8 Flags;     // ASS animation overwrite
    uint8 Dialogue;  // dialogue id
    uint8 Paragraph; // paragraph id
    uint8 Line;      // line (index) in dialogue

    SubtitleRect() { Reset(); }
    void Reset() {
        animationSet_.Reset();
        PrimaryColour = Color::White;
        SecondaryColour = Color::Red;
        OutlineColour = BackColour = Color::Black;
        TimeStart = TimeEnd = TimeKaraokeIn = TimeKaraokeOut = 0;
        Layer = 0;
        PlayResX = PlayResY = Width = Height = 0;
        X = Y = Xpin = Ypin = Xo = Yo = 0.0f;
        Yaw = Pitch = Roll = 0.0f;
        ScaleX = ScaleY = 1.0f;

        s0 = s1 = t0 = t1 = 0.0f;
        Shadow = 0;
        Border = 1; Alignment = 2;
        Karaoke = Flags = Dialogue = Paragraph = Line = 0;
    }

    // set move animation
    bool SetMoveXAnimation(sAnimationKey const* keys, int num) {
        return animationSet_.SetMoveXAnimation(keys, num);
    }
    bool SetMoveYAnimation(sAnimationKey const* keys, int num) {
        return animationSet_.SetMoveYAnimation(keys, num);
    }

    // set fade animation
    bool SetFadeAnimation(sAnimationKey const* keys, int num) { // num=4
        return animationSet_.SetFadeAnimation(keys, num);
    }

    // set size animation
    bool SetSizeAnimation(sAnimationKey const* keys, int num) { // num=4
        return animationSet_.SetSizeAnimation(keys, num);
    }/*
    bool SetScaleXAnimation(sAnimationKey const* keys, int num) { // num=4
        return animationSet_.SetScaleXAnimation(keys, num);
    }
    bool SetScaleYAnimation(sAnimationKey const* keys, int num) { // num=4
        return animationSet_.SetScaleYAnimation(keys, num);
    }*/

    // set roll animation
    bool SetRollAnimation(sAnimationKey const* keys, int num) { // num=4
        return animationSet_.SetRollAnimation(keys, num);
    }

    // get fade transparency
    int FadeAlpha(int t) const { return animationSet_.FadeAlpha(t); }

    // get extent... TO-DO : apply x, y offsets due to size animation
    bool GetExtent(float& x, float& y, float& xExt, float& yExt, int t) const {
        float dx(0.0f), dy(0.0f);
        animationSet_.GetMove(dx, dy, t);
        if (animationSet_.GetScale(xExt, yExt, t)) {
            //
            // Warning : if A dialogue is broken into several rects, dialogue size
            // will be changed with respect to all rects size. Hence the pinpoint
            // (Xpin, Ypin) will be chagned, and result could be wrong.
            // One way to fix this is to recalculate dialogue size to get updated pinpoint
            // via all rects' SubtitleRect::Dialogue, SubtitleRect::Paragraph and
            // SubtitleRect::Line. But it must be preformed by caller.
            //
            xExt *= ScaleX*Width;
            yExt *= ScaleY*Height;

            // adjust x, y to meet the pinpoint
            int const alignX = Alignment%3;
            int const alignY = (Alignment-1)/3;

            float Xref = X; // 1, 4, 7 : left aligned
            if (2==alignX) { // 2, 5, 8 : center aligned
                Xref = X + 0.5f*xExt;
            }
            else if (0==alignX) { // 3, 6, 9 : right aligned
                Xref = X + xExt;
            }

            float Yref = Y; // 7, 8, 9 : top
            if (0==alignY) { // 1, 2, 3 : bottom
                Yref = Y + yExt;
            }
            else if (1==alignY) { // 4, 5, 6 : center
                Yref = Y + 0.5f*yExt;
            }

            // new x and new y
            x = X + Xpin - Xref;
            y = Y + Ypin - Yref;
        }
        else {
            x = X; y = Y;
            xExt = ScaleX*Width;
            yExt = ScaleY*Height;
        }

        // apply offet (\move)
        x += dx;
        y += dy;

        return true;
    }

    // rotation angle in radian
    bool GetRotationParams(float& xOrg, float& yOrg,
                           float& yaw, float& roll, float& pitch, int t) const {
        yaw = Yaw; pitch = Pitch;
        if (!animationSet_.GetRotation(roll, t)) {
            roll = Roll;
        }

        if (0.0f!=roll || 0.0f!=pitch || 0.0f!=yaw) {
            if (0==(Flags&uint8(1<<4))) { // no \org(<x>, <y>), origin moves!
                animationSet_.GetMove(xOrg, yOrg, t);
                xOrg += Xo;
                yOrg += Yo;
            }
            else { // \org(<x>, <y>) apply, origin fixed!
                xOrg = Xo;
                yOrg = Yo;
            }
            return true;
        }

        return false;
    }
};

class Subtitle
{
public:
    enum { MAX_NUM_SUBTITLE_RECTS = 64 };

private:
    enum {
        // Font renderer constants
        TEXT_FONT_SIZE = 160,  // in pixels
        TEXT_FONT_DISTANCE_SPREAD_FACTOR = 16,
        TEXT_FONT_PYRAMID = 2, // downsizing (mipmap level, 0 for 1:1, 1 for 1:2...)
        TEXT_FONT_SPACE_GUARDBAND = (TEXT_FONT_SIZE>>TEXT_FONT_PYRAMID)/8, // to be texcoord offset safe(for shadow effects).

        // default test display height screen ratio, the default value.
        TEXT_DEFAULT_DISPLAY_HEIGHT_RATIO_PER_MILLI = 85, // 8.5%

        // ASS style
        MAX_SUBTITLE_STYLES = 64,
        MAX_ASS_STYLE_FIELDS = 64,
        MAX_ASS_DIALOGUE_FIELDS = 16, // #(fields)=10, as [ASS 4.0+]

        // max external subtitles(softsub) allow
        MAX_EXTERNAL_SUBTITLE_STREAMS = 32,
    };

    enum FORMAT {
        FORMAT_UNKNOWN,
        FORMAT_SRT,
        FORMAT_ASS,
    };

    // external subtitle stream
    struct SubStream {
        struct Dialogue {
            char const* TextStart;
            char const* TextEnd;
            int TimeStart, TimeEnd;
            // [ASS only]
            uint32 Style; // Hash32, 0 for default
            int Layer, MarginL, MarginR, MarginV;

            // for sorting
            bool operator<(Dialogue const& that) const {
                return TimeStart<that.TimeStart || (TimeStart==that.TimeStart && TimeEnd<that.TimeEnd);
            }

            void Reset() {
                TextStart = TextEnd = NULL;
                Layer = 0; Style = 0;
                TimeStart = TimeEnd = MarginL = MarginR = MarginV = -1;
            }
        };
        wchar_t Filename[32];
        mlabs::balai::Array<Dialogue> Dialogues;

        uint8*  Buffer; // data read from file
        FORMAT  Format;
        ISO_639 Language;
        uint32  Codepage; // file codepage(will convert all subtitle text to utf8)
        int     BufferSize;
        int     StyleLength; // ASS style lendth
        int     TimeStart;
        int     TimeEnd;
        int     NextDialogueId;

        SubStream():Dialogues(),Buffer(NULL),Format(FORMAT_UNKNOWN),
            Language(ISO_639_UNKNOWN),Codepage(0),BufferSize(0),
            StyleLength(0),
            TimeStart(0),TimeEnd(0),
            NextDialogueId(0) {
            memset(Filename, 0, sizeof(Filename));
        }

        void Reset() {
            memset(Filename, 0, sizeof(Filename));
            Dialogues.cleanup();
            if (NULL!=Buffer) {
                free(Buffer);
                Buffer = NULL;
            }
            Format = FORMAT_UNKNOWN;
            Language = ISO_639_UNKNOWN;
            Codepage = 0; 
            BufferSize = StyleLength = TimeStart = TimeEnd = NextDialogueId = 0;
        }
    };

    struct Style {
        char Fontname[system::FontParam::MAX_NAME_LENGTH]; // ASS field#2, utf8
        Color PrimaryColour;    // #4
        Color SecondaryColour;  // #5
        Color OutlineColour;    // #6
        Color BackColour;       // #7
        uint32 Name;      // #1, hash name, 0 for "Default"
        int    Fontsize;  // #3
        int    MarginL, MarginR, MarginV; // #14, #15, #16
        int    ScaleX, ScaleY; // #9.3, #9.4
        int    Encoding;  // #18, font character set. usually 0 for English(Western, ANSI) Windows
        float  Angle;     // #9.6, the origin of rotation is defined by the alignment [degrees]

        // font
        uint8  Bold;      // #8
        uint8  Italic;    // #9
        uint8  Underline; // #9.1
        uint8  StrikeOut; // #9.2
        uint8  Spacing;   // #9.6, extra space between characters [pixels]

        uint8  BorderStyle; // #10
        uint8  Outline;     // #11
        uint8  Shadow;      // #12
        uint8  Alignment;   // #13, will convert to V4+ style(layout of numpad)
        uint8  AlphaLevel;  // #17, transparency of the text. not present in ASS
        Style() { Reset(); }
        void Reset(char const* facename=NULL) {
            Name = 0;
            if (NULL!=facename) {
                while (*facename!='\0' && Name<system::FontParam::MAX_NAME_LENGTH) {
                    Fontname[Name++] = *facename++;
                }

                if (Name<system::FontParam::MAX_NAME_LENGTH) {
                    Fontname[Name] = '\0';
                }
                else {
                    Name = 0;
                }
            }

            if (0==Name) {
                memcpy(Fontname, "Arial", 6);
            }

            Name = 0;
            Fontsize = 20;
            PrimaryColour = Color::White;
            SecondaryColour = Color::Red;
            OutlineColour = BackColour = Color::Black;

            Bold = Italic = Underline = StrikeOut = 0;
            ScaleX = ScaleY = 100;
            Spacing = 0;
            Angle = 0.0f;

            // ASS style default = outline + shadow
            BorderStyle = 1; // 1=outline + drop shadow, 3=Opaque box
            Outline = 1; // [0, 4], width of the outline around the text [pixels] (if BorderSize is 1)
            Shadow = 1;  // [0, 4], depth of the drop shadow behind the text [pixels] (if BorderSize is 1)

            Alignment = 2; // 2=bottom centered (as numpad layout)
            MarginL = MarginR = MarginV = 0; // [pixels]

            AlphaLevel = 0; // full opacity
            Encoding = 0;   // ANSI
        }

        bool BuildFontParam(system::FontParam& param) const {
            memcpy(param.name, Fontname, system::FontParam::MAX_NAME_LENGTH);
            param.size = TEXT_FONT_SIZE;
            param.bold = Bold; // 0, 1, 2, 3
            param.italic = Italic;
            param.underline = Underline;
            param.strikeout = StrikeOut;
            return true;
        }

        // check if specify font is available(installed)
        bool CheckFontFace(ISO_639 lan, int codepage) const {
            system::FontParam param;
            memcpy(param.name, Fontname, system::FontParam::MAX_NAME_LENGTH);
            param.size = TEXT_FONT_SIZE;
            param.bold = Bold; // 0, 1, 2, 3
            param.italic = Italic;
            param.underline = Underline;
            param.strikeout = StrikeOut;
            return IsFontAvailable(param, lan, codepage);
        }
    };

    SubStream   extSubtitleStreams_[MAX_EXTERNAL_SUBTITLE_STREAMS];
    Style       styles_[MAX_SUBTITLE_STYLES];
    uint32      dialogueFields_[MAX_ASS_DIALOGUE_FIELDS];
    std::mutex  mutex_;
    system::TextBlitter textBlitter_;
    int         totalStyles_;          // total ASS/SSA styles
    int         defaultStyle_;         //
    int         totalDialogueFields_;  // total ASS/SSA dialogue fields, or 10
    int         totalExtSubtitleStreams_;
    int         currentExtSubtitleStreamId_;
    int         movieResX_;
    int         movieResY_;

    //
    // ASS PlayResX: PlayResY:
    int playResX_, playResY_;

    //
    // ASS WrapStyle:
    // 0 : smart wrapping, lines are evently broken
    // 1 : end-of-line word wrapping, only \N breaks
    // 2 : no word wrapping, \n \N both breaks
    // 3 : same as 0, but lower line get wider
    int wrapMode_;

    //
    // ASS Collisions: To determine how subtitles are positioned.
    //
    // "Normal"  : Subtitles will "stack up" one above the other. They always
    //             be positioned as close the vertical margin as possible.
    //
    // "Reverse" : Subtitles will be shifted upwards to make room for
    //             subsequence oeverlapping subtitles. This means subtitles
    //             can nearly always be read top-down. But it also means that
    //             the first subtitle can appear half way up the screen
    //             before the subsequent overlapping subtitles appear.
    //             It can use a lot of screen area.
    //
    enum { COLLISION_NORMAL = 1,
           COLLISION_REVERSE = 2,
           COLLISION_DEFAULT = COLLISION_NORMAL
    } collisions_;

    bool SetFont_(system::FontParam& param, ISO_639 lan) {
        return textBlitter_.Create(param, lan, TEXT_FONT_DISTANCE_SPREAD_FACTOR);
    }
    bool SetFont_(system::FontParam& param, int encoding) {
        return textBlitter_.Create(param, encoding, TEXT_FONT_DISTANCE_SPREAD_FACTOR);
    }
    bool DialogueLineBreaker_(int& texture_width, int& texture_height, int& display_left,
                              int& text_length, int& text_linebreaks,
                              wchar_t const* ptr, wchar_t const* end,
                              int display_height, int max_display_width);

    bool ParseSubtitleStream_(SubStream& sub, FORMAT format) const;
    bool AddSubtitleStream_(wchar_t const* filename, FORMAT format);

    // ASS/SSA styles
    int ASS_Styles_(char const* begin, char const* end, ISO_639 lan, int encoding=CP_UTF8);

    // dialogue from text
    bool ASS_Dialogue_(SubStream::Dialogue& dialogue, char const* text,
                       uint32 const* dialogue_fields, int total_dialogue_fields) const;

    //
    // Note:timestamp will clamp dialogue's TimeStart.
    // (set timestamp=0 if you don't want to)
    int SRT_Dialogue_Text_(uint8* buffer, int buffer_size, int& width, int& height,
                           SubtitleRect* rects, int max_rects,
                           SubStream::Dialogue const* dialogues, int totals,
                           int timestamp, int encoding=CP_UTF8);

    int ASS_Dialogue_Text_(uint8* buffer, int buffer_size, int& width, int& height,
                           SubtitleRect* rects, int max_rects,
                           SubStream::Dialogue const* dialogues, int totals,
                           int timestamp, int encoding=CP_UTF8);
public:
    Subtitle();
    ~Subtitle() { Finalize(); }

    void SetMoiveResolution(int w, int h) {
        assert(w>0 && h>0);
        movieResX_ = w;
        movieResY_ = h;
    }

    bool Initialize();
    void Finalize();

    //
    int Create(char const* videofile);
    void Destroy();

    // ready to play
    void ClearCache() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (int i=0; i<totalExtSubtitleStreams_; ++i) {
            extSubtitleStreams_[i].NextDialogueId = 0;
        }
    }

    int TotalStreams() const { return totalExtSubtitleStreams_; }
    int GetSubtitleStreamInfo(int streamIDs[], ISO_639 languages[], int max_count) const {
        assert(max_count>0);
        int got_subs = 0;
        for (int i=0; i<totalExtSubtitleStreams_&&got_subs<max_count; ++i) {
            SubStream const& sub = extSubtitleStreams_[i];
            streamIDs[got_subs] = i;
            languages[got_subs] = sub.Language;
            ++got_subs;
        }
        return got_subs;
    }

    bool OnChangeExternalSubtitle(int subtitle_id) {
        if (0<=subtitle_id && subtitle_id<totalExtSubtitleStreams_) {
            std::lock_guard<std::mutex> lock(mutex_);
            SubStream& sub = extSubtitleStreams_[subtitle_id];
            sub.NextDialogueId = 0;
            if (sub.Format==FORMAT_ASS) {
                collisions_ = COLLISION_DEFAULT;
                wrapMode_ = 0;
                playResX_ = movieResX_;
                playResY_ = movieResY_;
                totalStyles_ = 0;
                char const* begin = (char const*) sub.Buffer;
                defaultStyle_ = ASS_Styles_(begin, begin + sub.StyleLength, sub.Language, sub.Codepage);
                if (defaultStyle_<0) {
                    BL_LOG("Not a ASS subtitle!?\n");
                    styles_[0].Reset();
                    totalStyles_ = 1;
                    defaultStyle_ = 0;
                }

                // set default style
                system::FontParam font_settings;
                styles_[defaultStyle_].BuildFontParam(font_settings);
                SetFont_(font_settings, sub.Codepage);

                currentExtSubtitleStreamId_ = subtitle_id;
                return true;
            }
            else if (sub.Format==FORMAT_SRT) {
                collisions_ = COLLISION_DEFAULT;
                wrapMode_ = 0;
                playResX_ = movieResX_;
                playResY_ = movieResY_;

                // set default style
                //char const* fontname = "Calibri";
                //char const* fontname = "Arial"; // ASS default font name
                char const* fontname = "Microsoft YaHei";
                styles_[0].Reset(fontname);
                if (!styles_[0].CheckFontFace(sub.Language, sub.Codepage)) {
                    styles_[0].Reset("Arial");
                }
                system::FontParam font_settings;
                styles_[0].BuildFontParam(font_settings);
                SetFont_(font_settings, sub.Codepage);

                totalStyles_ = 1;
                defaultStyle_ = 0;

                currentExtSubtitleStreamId_ = subtitle_id;
                return true;
            }
        }
        return false;
    }

    //
    // SSA(Sub Station Alpha)/ASS(Advance Sub Station Alpha) style string
    //  https://www.matroska.org/technical/specs/subtitles/ssa.html
    //  (In Matroska, All text is converted to UTF-8)
    //
    int LoadStyles(char const* style, int stylelength, ISO_639 lan);

    //
    // embedded subtitle(AVSubtitleType = SUBTITLE_ASS)
    // return number of rows if succeed, 0 for not recognized ass or NULL,
    // negative value(=-width*height) for buffer_size too small.
    // (In Matroska, ass must be UTF-8)
    int Dialogue_ASS(uint8* buffer, int buffer_size, int& width, int& height,
                     SubtitleRect* rects, int max_rects, char const* ass);

    //
    // embedded subtitle(AVSubtitleType = SUBTITLE_TEXT)
    // To be implemented. we still cannot find video sources with it...
    int Dialogue_TEXT(uint8* /*buffer*/, int /*buffer_size*/, int& /*width*/, int& /*height*/,
                      SubtitleRect* /*rects*/, int /*max_rects*/, char const* text) {
        std::lock_guard<std::mutex> lock(mutex_);
        BL_LOG("Dialogue_TEXT() no implement! %s\n", text);
        return 0;
    }

    // external subtitle
    bool IsFinish(int subtitleId, int timestamp);

    // return # of rects
    int Publish(int subtitleId, int timestamp,
                uint8* buffer, int buffer_size, int& width, int& height,
                SubtitleRect* rects, int max_rects);

    // UGLY band aid! C++ experts, close your eyes please~~
    void* CookExtSubtitleNameTexture(int& width, int& height,
                                     int tc[][4], int* subStmIDs_, int idOffset, int total_subtitles) const {
        width = height = 0;
        if (totalExtSubtitleStreams_>0) {
            char const* font_name = "Calibri"; // to make Korean shown properly
            system::TextBlitter fontBlitter;
            if (!fontBlitter.Create(font_name, 160, true, extSubtitleStreams_[0].Codepage, 16)) {
                return NULL;
            }

            int const max_length = sizeof(extSubtitleStreams_[0].Filename)/sizeof(extSubtitleStreams_[0].Filename[0]);
            wchar_t filenames[MAX_EXTERNAL_SUBTITLE_STREAMS][max_length];
            int length[MAX_EXTERNAL_SUBTITLE_STREAMS] = { 0 };
            int ww(0), hh(0);
            for (int i=0; i<total_subtitles; ++i) {
                int const id = subStmIDs_[i] - idOffset;
                if (0<=id && id<totalExtSubtitleStreams_) {
                    int& len = length[id];
                    wchar_t const* fname = extSubtitleStreams_[id].Filename;
                    wchar_t const* fname_end = fname + max_length;
                    while (fname<fname_end && (L'.'==*fname || L' '==*fname)) {
                        ++fname;
                    }

                    wchar_t* name = filenames[id];
                    for (len=0; fname<fname_end && 0!=*fname; ) {
                        name[len++] = *fname++;
                    }

                    if (0<len) {
                        if (len>16) {
                            memmove(name, name+(len-16), 16*sizeof(wchar_t));
                            name[0] = name[1] = name[2] = '.';
                            len = 16;
                            while (' '==name[3] && len>3) {
                                memmove(name+3, name+4, (len-4)*sizeof(wchar_t));
                                --len;
                            }
                        }
                        else if (3==len) {
                            // the simplest subtitle file name. upper case to be more readable.
                            if ('s'==name[0] && 'r'==name[1] && 't'==name[2]) {
                                name[0] = 'S';
                                name[1] = 'R';
                                name[2] = 'T';
                            }
                            else if ('a'==name[0] && 's'==name[1] && 's'==name[2]) {
                                name[0] = 'A';
                                name[1] = 'S';
                                name[2] = 'S';
                            }
                        }
                        fontBlitter.GetTextExtend(ww, hh, 2, true, name, len);
                        tc[i][0] = ww;
                        tc[i][1] = hh;
                        if (width<ww)
                            width = ww;
                    }
                    else {
                        tc[i][0] = tc[i][1] = 0;
                    }
                }
                else {
                    tc[i][0] = tc[i][1] = 0;
                }
                tc[i][2] = tc[i][3] = 0;
            }

            width = (width+3) & ~3;
            if (width<512)
                width = 512;

            height = ww = hh = 0;
            for (int i=0; i<total_subtitles; ++i) {
                int const id = subStmIDs_[i] - idOffset;
                if (0<=id && id<totalExtSubtitleStreams_) {
                    if ((ww+=tc[i][0])>width) {
                        height += hh;
                        ww = tc[i][0];
                        hh = tc[i][1];
                    }
                    else if (hh<tc[i][1]) {
                        hh = tc[i][1];
                    }
                }
            }
            height += hh;

            uint8* const pixels = (uint8*) malloc(width*height);
            memset(pixels, 0, width*height);
            if (pixels) {
                int s0(0), t0(0), sx(0), sy(0), ix(0), iy(0);
                hh = 0;
                for (int i=0; i<total_subtitles; ++i) {
                    int const id = subStmIDs_[i] - idOffset;
                    if (0<=id && id<totalExtSubtitleStreams_) {
                        if ((s0+tc[i][0])>width) {
                            t0 += hh;
                            s0 = hh = 0;
                        }

                        sx = width - s0;
                        sy = tc[i][1];
                        if (fontBlitter.DistanceMap(pixels + width*t0 + s0,
                                                    sx, sy, ix, iy, width, 2, filenames[id], length[id])) {
                            tc[i][0] = s0;
                            tc[i][1] = s0 += sx;
                            tc[i][2] = t0;
                            tc[i][3] = t0 + sy;
                            if (hh<sy)
                                hh = sy;
                        }
                        else {
                            tc[i][0] = tc[i][1] = tc[i][2] = tc[i][3] = 0;
                        }
                    }
                    else {
                        tc[i][0] = tc[i][1] = tc[i][2] = tc[i][3] = 0;
                    }
                }

                if (0==hh) {
                    free(pixels);
                    return NULL;
                }
                return pixels;
            }
        }
        return NULL;
    }
};

}}}

#endif