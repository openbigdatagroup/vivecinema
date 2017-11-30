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
 * @file    Subtitle.cpp
 * @author  andre chen
 * @history 2016/08/25 created
 *
 */
#include "Subtitle.h"
#include "uchardet.h"

//#define USE_GOOGLE_COMPACT_LANGUAGE_DETECTOR
#ifdef USE_GOOGLE_COMPACT_LANGUAGE_DETECTOR
//
// google's Compact Language Detector 2
//
// !!! the app size goes up from 916KB to 2240KB !!!
//
#include "public/compact_lang_det.h"
#endif

// debug and test
//#include "BLPNG.h"

// Extended Linguistic Services
#include <elscore.h> 
#include <elssrvc.h>

using mlabs::balai::system::FontParam;

namespace mlabs { namespace balai { namespace video {

// [V4+ Styles] table
static uint32 const Style_Name = mlabs::balai::CalcCRC("Name");
static uint32 const Style_Fontname = mlabs::balai::CalcCRC("Fontname");
static uint32 const Style_Fontsize = mlabs::balai::CalcCRC("Fontsize");
static uint32 const Style_PrimaryColour = mlabs::balai::CalcCRC("PrimaryColour");
static uint32 const Style_SecondaryColour = mlabs::balai::CalcCRC("SecondaryColour");
static uint32 const Style_OutlineColour = mlabs::balai::CalcCRC("OutlineColour");
static uint32 const Style_BackColour = mlabs::balai::CalcCRC("BackColour");
static uint32 const Style_Bold = mlabs::balai::CalcCRC("Bold");
static uint32 const Style_Italic = mlabs::balai::CalcCRC("Italic");
static uint32 const Style_Underline = mlabs::balai::CalcCRC("Underline");
static uint32 const Style_StrikeOut = mlabs::balai::CalcCRC("StrikeOut"); // real case
static uint32 const Style_Strikeout = mlabs::balai::CalcCRC("Strikeout"); // by document
static uint32 const Style_ScaleX = mlabs::balai::CalcCRC("ScaleX");
static uint32 const Style_ScaleY = mlabs::balai::CalcCRC("ScaleY");
static uint32 const Style_Spacing = mlabs::balai::CalcCRC("Spacing");
static uint32 const Style_Angle = mlabs::balai::CalcCRC("Angle");
static uint32 const Style_BorderStyle = mlabs::balai::CalcCRC("BorderStyle");
static uint32 const Style_Outline = mlabs::balai::CalcCRC("Outline");
static uint32 const Style_Shadow = mlabs::balai::CalcCRC("Shadow");
static uint32 const Style_Alignment = mlabs::balai::CalcCRC("Alignment");
static uint32 const Style_MarginL = mlabs::balai::CalcCRC("MarginL");
static uint32 const Style_MarginR = mlabs::balai::CalcCRC("MarginR");
static uint32 const Style_MarginV = mlabs::balai::CalcCRC("MarginV");
static uint32 const Style_AlphaLevel = mlabs::balai::CalcCRC("AlphaLevel");
static uint32 const Style_Encoding = mlabs::balai::CalcCRC("Encoding");

//[Events]
//Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text
static uint32 const Dialogue_Marked = mlabs::balai::CalcCRC("Marked"); // [V4.00]
static uint32 const Dialogue_Layer = mlabs::balai::CalcCRC("Layer");   // [V4.00+]
static uint32 const Dialogue_Start = mlabs::balai::CalcCRC("Start");
static uint32 const Dialogue_End = mlabs::balai::CalcCRC("End");
static uint32 const Dialogue_Style = mlabs::balai::CalcCRC("Style");
static uint32 const Dialogue_Name = Style_Name;
static uint32 const Dialogue_MarginL = Style_MarginL;
static uint32 const Dialogue_MarginR = Style_MarginR;
static uint32 const Dialogue_MarginV = Style_MarginV;
static uint32 const Dialogue_Effect = mlabs::balai::CalcCRC("Effect");
static uint32 const Dialogue_Text = mlabs::balai::CalcCRC("Text");

static uint32 const Dialogue_Style_Default = mlabs::balai::CalcCRC("Default");
static uint32 const Dialogue_Style_StarDefault = mlabs::balai::CalcCRC("*Default");

//---------------------------------------------------------------------------------------
inline char const* newline(char const* str, char const* end) {
    while ((*str=='\r' || *str=='\n') && str<end) {
        ++str;
    }
    return str;
}
inline char const* eatspace(char const* str, char const* end) {
    while (*str==' ' && str<end) {
        ++str;
    }
    return str;
}
inline bool isdigit(char c) { return '0'<=c && c<='9'; }
inline uint8 hex(char a, char b) {
    int s = -1;
    if ('0'<=a && a<='9') {
        s = int(a-'0')<<4;
    }
    else if ('A'<=a && a<='F') {
        s = (10+int(a-'A'))<<4;
    }
    else if ('a'<=a && a<='f') {
        s = (10+int(a-'a'))<<4;
    }
    else {
        return 0;
    }

    if ('0'<=b && b<='9') {
        s += int(b-'0');
    }
    else if ('A'<=b && b<='F') {
        s += 10+int(b-'A');
    }
    else if ('a'<=b && b<='f') {
        s += 10+int(b-'a');
    }
    else {
        return 0;
    }

    return (uint8) s;
}
inline void interpret_color(Color& clr, char const* s, char const* e)
{
    if ((s+2)<e && s[0]=='&' && s[1]=='H') {
        s += 2;

        // aabbggrr, aa = transparency not opacity
        int const digits = (int)(e - s);
        if (digits<=8) {
            if (digits>=7) {
                clr.a = 255 - hex(digits>7 ? e[-8]:'0', e[-7]);
            }
            else {
                // Don't change alpha here, for \1-4c do not contains transparency!
                //clr.a = 0xff;
            }

            if (digits>=5) {
                clr.b = hex(digits>5 ? e[-6]:'0', e[-5]);
            }
            else {
                clr.b = 0;
            }

            if (digits>=3) {
                clr.g = hex(digits>3 ? e[-4]:'0', e[-3]);
            }
            else {
                clr.g = 0;
            }

            clr.r = hex(digits>1 ? e[-2]:'0', e[-1]);
        }
    }
    else if (s<e && isdigit(*s)) {
        int const color = std::atol(s); // ???
        clr.r = (uint8) (color&0xff);
        clr.g = (uint8) ((color&0xff00)>>8);
        clr.b = (uint8) ((color&0xff0000)>>16);
        clr.a = 255; // not really sure!!!
    }
}
inline int srt_timestamp(char const* s, char const* e) {
    if ((s+8)<e && ':'==s[2] && ':'==s[5] && ('.'==s[8]||','==s[8]||':'==s[8])) {
        int h1 = s[0] - '0';
        int h2 = s[1] - '0';
        int m1 = s[3] - '0';
        int m2 = s[4] - '0';
        int s1 = s[6] - '0';
        int s2 = s[7] - '0';
        int ms1 = ((s+9)<e) ? (s[9] - '0'):0;
        int ms2 = ((s+10)<e) ? (s[10] - '0'):0;
        int ms3 = ((s+11)<e) ? (s[11] - '0'):0;
        if (0<=h1 && h1<10 &&
            0<=h2 && h2<10 &&
            0<=m1 && m1<10 &&
            0<=m2 && m2<10 &&
            0<=s1 && s1<10 &&
            0<=s2 && s2<10 &&
            0<=ms1 && ms1<10 &&
            0<=ms2 && ms2<10 &&
            0<=ms3 && ms3<10) {
            return (h1*36000 + h2*3600 + m1*600 + m2*60 + s1*10 + s2)*1000 + ms1*100 + ms2*10 + ms3;
        }
    }
    return -1;
}

inline bool srt_timestamp(int& time_start, int& time_end, char*& text, char* s, char const* end) {
    //
    // normally be
    //      00:00:00,009 --> 00:00:05,230
    //
    // but sometimes you get
    //      00:00:00.009 --> 00:00:05.230
    // or
    //      00:00:00,00 --> 00:00:05,23
    // or even worse
    //      1 00
    //      :00:00.009 --> 00:00:05.230
    //
    char time_buffer[13];
    int t(0), lines(0);
    char* timecode = time_buffer + 1;
    while (s<end && t<12 && '-'!=*s && lines<=1) {
        if ('\n'!=*s) {
            if (' '!=*s && '\r'!=*s) {
                timecode[t++] = *s;
            }
        }
        else {
            ++lines;
        }
        ++s;
    }

    if (':'==timecode[1]) {
        time_buffer[0] = '0';
        timecode = time_buffer;
        ++t;
    }
    time_start = srt_timestamp(timecode, timecode+t);
    if (time_start<0)
        return false;

    t = 0;
    while (s<end && t<3) {
        if (' '!=*s && '\r'!=*s && '\n'!=*s) {
            if (t<2) {
                if ('-'!=*s)
                    break;
                ++t;
            }
            else {
                if ('>'!=*s)
                    break;
                ++t;
            }
        }
        ++s;
    }

    if (t<3)
        return false;

    timecode = time_buffer + 1;
    for (t=0; s<end&&t<12&&'\n'!=*s; ) {
        if (' '!=*s && '\r'!=*s) {
            timecode[t++] = *s;
        }
        ++s;
    }

    if (':'==timecode[1]) {
        time_buffer[0] = '0';
        timecode = time_buffer;
        ++t;
    }

    time_end = srt_timestamp(timecode, timecode+t);
    if (time_start<time_end) {
        while (s<end && ' '==*s) ++s;
        text = s;
        return true;
    }

    return false;
}

// begin should be pointer to each subtitle, a numeric counter identifying each sequential subtitle.
inline bool srt_subtitle_start(int& id, int& time_start, int& time_end, char*& begin, char const* end) {
    if ((begin+30)<end && (0!=(id=atol(begin))||'0'==*begin)) {
        char* s = begin + 1;
        while (s<end && isdigit(*s)) ++s;
        if (srt_timestamp(time_start, time_end, begin, s, end)) {
            while (begin<end && ' '==*begin) {
                ++begin;
            }
            return true;
        }
    }
    return false;
}

// search next subtitle start(and hence this subtitle end)
inline char* srt_subtitle_seek_next(int& id, int& time_start, int& time_end, int& lines, char*& s, char const* end) {
    int chars = lines = 0;
    char* sub_end = s;
    while (s<end) {
        if (*s!='\r' && *s!='\n') {
            if (sub_end<s) {
                *sub_end = *s;
            }
            ++sub_end; ++s;
            ++chars;
        }
        else {
            if (s[0]=='\r' && s[1]=='\n')
                s += 2;
            else
                ++s;

            if (0<lines && 0==chars && srt_subtitle_start(id, time_start, time_end, s, end)) {
                *sub_end = '\0'; // '\n' -> '\0'
                return sub_end;
            }
            else {
                *sub_end++ = '\n';
            }
            ++lines;
            chars = 0;
        }
    }

    *sub_end = '\0';
    return sub_end;
}

template<typename endian_trait>
int ConvertUTF32toUTF8(uint8*& dst_start, uint8* dst_end,
                       uint8 const*& src_start, uint8 const* src_end)
{
    uint32 const UNI_SUR_HIGH_START = (uint32) 0xD800;
//    uint32 const UNI_SUR_HIGH_END   = (uint32) 0xDBFF;
//    uint32 const UNI_SUR_LOW_START  = (uint32) 0xDC00;
    uint32 const UNI_SUR_LOW_END    = (uint32) 0xDFFF;
    uint32 const UNI_MAX_LEGAL_UTF32 = (uint32) 0x0010FFFF;
    uint32 const byteMask = 0xBF;
    uint32 const byteMark = 0x80;
    uint8 const firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };

    uint32 cp;
    int ret = 0;
    while (dst_start<dst_end && (src_start+4)<src_end) {
        cp = endian_trait::cp(src_start);
        if (cp>=UNI_SUR_HIGH_START && cp<=UNI_SUR_LOW_END) {
            return -ret;
        }

        int bytesToWrite = 0;
        if (cp<(uint32)0x80) {
            bytesToWrite = 1;
        }
        else if (cp<(uint32)0x800) {
            bytesToWrite = 2;
        }
        else if (cp<(uint32)0x10000) {
            bytesToWrite = 3;
        }
        else if (cp<=UNI_MAX_LEGAL_UTF32) {
            bytesToWrite = 4;
        }
        else {
            return -ret;
        }

        if ((dst_start+bytesToWrite)<dst_end) {
            switch (bytesToWrite) { /* note: everything falls through. */
                case 4: dst_start[3] = (uint8)((cp | byteMark) & byteMask); cp >>= 6;
                case 3: dst_start[2] = (uint8)((cp | byteMark) & byteMask); cp >>= 6;
                case 2: dst_start[1] = (uint8)((cp | byteMark) & byteMask); cp >>= 6;
                case 1: dst_start[0] = (uint8) (cp | firstByteMark[bytesToWrite]);
            }

            ret += bytesToWrite;
            dst_start += bytesToWrite;
            src_start += 4;
        }
        else {
            return -ret;
        }
    }

    return ret;
}

template<typename endian_trait>
int GetUTF32toUTF8Size(uint8 const*& src_start, uint8 const* src_end)
{
    uint32 const UNI_SUR_HIGH_START  = (uint32) 0xD800;
    uint32 const UNI_SUR_LOW_END     = (uint32) 0xDFFF;
    uint32 const UNI_MAX_LEGAL_UTF32 = (uint32) 0x0010FFFF;
    uint32 cp;
    int ret = 0;
    while ((src_start+4)<src_end) {
        cp = endian_trait::cp(src_start);
        if (cp>=UNI_SUR_HIGH_START && cp <= UNI_SUR_LOW_END) {
            return -ret;
        }

        if (cp<(uint32)0x80) {
            ++ret;
        }
        else if (cp<(uint32)0x800) {
            ret += 2;
        } else if (cp < (uint32)0x10000) {
            ret += 3;
        } else if (cp <= UNI_MAX_LEGAL_UTF32) {
            ret += 4;
        }
        else {
            return -ret;
        }

        src_start += 4;
    }

    return ret;
}

//---------------------------------------------------------------------------------------
Subtitle::Subtitle():
mutex_(),
textBlitter_(),
totalStyles_(0),defaultStyle_(-1),
totalDialogueFields_(0),
totalExtSubtitleStreams_(0),currentExtSubtitleStreamId_(-1),
movieResX_(0),movieResY_(0),
playResX_(0),playResY_(0),
wrapMode_(0),collisions_(COLLISION_DEFAULT)
{
}
//---------------------------------------------------------------------------------------
bool Subtitle::ParseSubtitleStream_(SubStream& sub, FORMAT format) const
{
    if (NULL==sub.Buffer || sub.BufferSize<=0)
        return false;

    SubStream::Dialogue dial;
    sub.Dialogues.cleanup();
    sub.TimeEnd = sub.TimeStart = 0;
    char const* const begin = (char const*) sub.Buffer;
    char const* const end = begin + sub.BufferSize;
    if (format==FORMAT_SRT) {
        int const reserve_dialogues = sub.BufferSize/32;
        sub.Dialogues.reserve(reserve_dialogues<2048 ? reserve_dialogues:2048);
        dial.Reset();

        //
        // each subtitle consists of four parts, all in text..
        //
        // 1. A number indicating which subtitle it is in the sequence.
        // 2. The time that the subtitle should appear on the screen, and then disappear.
        // 3. The subtitle itself.
        // 4. A blank line indicating the start of a new subtitle.
        //
        // an example SRT file:
        //
        //
        // 1
        // 00:02:17,440 --> 00:02:20,375
        // Senator, we're making
        // our final approach into Coruscant.
        //
        // 2
        // 00:02:20,476 --> 00:02:22,501
        // Very good, Lieutenant.
        //
        //
        // references :
        // http://forum.doom9.org/showthread.php?p=470941#post470941
        // https://www.matroska.org/technical/specs/subtitles/srt.html
        //
        int seq(-1), lines(0), time_start(0), time_end(0);
        char* s = (char*) begin;
        if (!srt_subtitle_start(seq, time_start, time_end, s, end))
            return false;

#ifdef BL_DEBUG_BUILD
        int seq2 = seq;
#endif
        while (s<end) {
            dial.TimeStart = time_start;
            dial.TimeEnd = time_end;

            // move to the end of timecode line
            char* e = s;
            while (e<end && *e!='\r' && *e!='\n') {
                ++e;
            }

            //
            // rest part of timecode line is optional position, X1:40 X2:600 Y1:20 Y2:50
            // but it's hard to support in reasonable sense.
            //
            dial.MarginL = dial.MarginR = dial.MarginV = 0;
            if ((s+20)<e && s[0]=='X' && s[1]=='1' && s[2]==':') {
                char* left = s+3;
                char* right = left+1;
                while ((right+4)<e && right[0]!='X' && right[1]!='2' && right[2]!=':') {
                    ++right;
                }

                char* top(NULL), *bottom(NULL);
                if ((right+4)<e) {
                    right += 3;
                    top = right + 1;
                    while ((top+4)<e && top[0]!='Y' && top[1]!='1' && top[2]!=':') {
                        ++top;
                    }

                    if ((top+4)<e) {
                        top += 3;
                        bottom = top + 1;
                        while ((bottom+4)<e && bottom[0]!='Y' && bottom[1]!='2' && bottom[2]!=':') {
                            ++bottom;
                        }

                        if ((bottom+4)<e) {
                            bottom += 3;
                        }
                        else {
                            bottom = NULL;
                        }
                    }
                }

                if (NULL!=bottom) {
                    //dial.Left = atol(left);
                    //dial.Top = atol(top);
                    //dial.Height = atol(bottom) - dial.Top;
                }
            }

            if (e[0]=='\r' && e[1]=='\n')
                dial.TextStart = s = e+2;
            else
                dial.TextStart = s = ++e;

            dial.TextEnd = srt_subtitle_seek_next(seq, time_start, time_end, lines, s, end);
            if (dial.TextStart<dial.TextEnd) {
                // a nasty case ///////////////////////////
                if (lines>10) { 
                    lines = 0;
                    e = (char*) dial.TextStart;
                    while (e<dial.TextEnd) {
                        if ('\n'==*e && ++lines>=10) {
                            *e = '\0';
                            dial.TextEnd = e;
                            break;
                        }
                        ++e;
                    }
                }
                ///////////////////////////////////////////

                e = (char*) dial.TextEnd - 1;
                while (dial.TextStart<e && (*e=='\r' || *e=='\n' || *e==' ')) {
                    dial.TextEnd = e;
                    *e-- = '\0';
                }

                while (dial.TextStart<dial.TextEnd && ' '==*dial.TextStart) {
                    ++dial.TextStart;
                }

                if (dial.TextStart<dial.TextEnd) {
                    if (!sub.Dialogues.empty()) {
                        if (sub.TimeStart>dial.TimeStart)
                            sub.TimeStart = dial.TimeStart;
                        if (sub.TimeEnd<dial.TimeEnd)
                            sub.TimeEnd = dial.TimeEnd;
                    }
                    else {
                        sub.TimeStart = dial.TimeStart;
                        sub.TimeEnd = dial.TimeEnd;
                    }
                    sub.Dialogues.push_back(dial);
                }
            }

#ifdef BL_DEBUG_BUILD
            if (s<end) {
                if ((seq2+1)!=seq) {
                    //BL_LOG("SRT sequence out-of-order %d -> %d\n", seq2, seq);
                }
                seq2 = seq;
            }
#endif
        }
    }
    else if (format==FORMAT_ASS) {
        if (0!=memcmp(begin, "[Script Info]", 13))
            return false;

        char const* styles = NULL;
        char const* events = NULL;
        char const* s = begin + 13;
        while (NULL==events && s<end) {
            char const* e = s + 1;
            while (*e!='\r' && *e!='\n' && e<end) {
                ++e;
            }

            if (*s=='[') {
                if (NULL==styles) {
                    // [V4+ Styles], [v4+ Styles], [V4+ styles], [v4+ styles],
                    // [V4 Styles], [v4 Styles], [V4 styles], [v4 styles]
                    if (('v'==s[1] || 'V'==s[1]) && '4'==s[2]) {
                        events = s;
                        s += ('+'==s[3]) ? 4:3;
                        if (' '==*s++ && ('s'==*s||'S'==*s) && 0==memcmp(s+1, "tyles]", 6)) {
                            styles = events;
                        }
                        events = NULL;
                    }
                }
                else if (0==memcmp(s+1, "Events]", 7)) {
                    events = s;
                }
            }

            if (e[0]=='\r' && e[1]=='\n') {
                s = e + 2;
            }
            else {
                s = e + 1;
            }
        }

        if (NULL==events || NULL==styles) {
            return false;
        }

        // format and dialog. s now pointer to next line of [Events]
        char const* format = NULL;
        uint32 dialogue_fields[MAX_ASS_DIALOGUE_FIELDS];
        int total_dialogue_fields = 0;
        while (NULL==format && s<end) {
            char const* e = s + 1;
            while (*e!='\r' && *e!='\n' && e<end) {
                ++e;
            }

            if (0==memcmp(s, "Format:", 7)) {
                format = s;
                sub.StyleLength = (int) ((uint8 const*)e - sub.Buffer);
                s += 7;
                while (s<e) {
                    while (' '==*s && s<e) ++s; // eat space
                    char const* e1 = s + 1;
                    while (*e1!=',' && e1<e) ++e1;

                    if (s<e1 && total_dialogue_fields<MAX_ASS_DIALOGUE_FIELDS) {
                        dialogue_fields[total_dialogue_fields++] = mlabs::balai::CalcCRC((uint8*)s, int(e1-s));
                    }

                    s = e1 + 1;
                }
            }

            if (e[0]=='\r' && e[1]=='\n') {
                s = e + 2;
            }
            else {
                s = e + 1;
            }
        }

        // format loaded
        if (total_dialogue_fields<10) {
            sub.StyleLength = 0;
            return false;
        }

        if (total_dialogue_fields!=10) {
            // !!!
        }

        // parse all dialogues, one line after the other.
        int const reserve_dialogues = (sub.BufferSize - sub.StyleLength)/64;
        sub.Dialogues.reserve(reserve_dialogues<2048 ? reserve_dialogues:2048);

        while (s<end) {
            char const* e = s + 1;
            while (*e!='\r' && *e!='\n' && e<end) {
                ++e;
            }

            char const* s0 = (char*) s;
            if (e[0]=='\r' && e[1]=='\n') {
                s = e + 2;
            }
            else {
                s = e + 1;
            }

            if ((s0+10)<e && 0==memcmp(s0, "Dialogue:", 9)) {
                char* e0 = (char*) e;
                *e0 = '\0';

                if (ASS_Dialogue_(dial, s0+9, dialogue_fields, total_dialogue_fields)) {
                    e0 = (char*) dial.TextEnd;
                    *e0-- = '\0';

                    while (dial.TextStart<=e0) {
                        if (*e0=='\r' || *e0=='\n' || *e0==' ') {
                            dial.TextEnd = e0;
                            *e0-- = '\0';
                        }
                        else if (dial.TextStart<e0 && *e0=='N' && e0[-1]=='\\') {
                            dial.TextEnd = --e0;
                            *e0-- = '\0';
                        }
                        else {
                            break;
                        }
                    }

                    while (dial.TextStart<dial.TextEnd && ' '==*dial.TextStart) {
                        ++dial.TextStart;
                    }

                    if (dial.TextStart<dial.TextEnd) {
                        if (!sub.Dialogues.empty()) {
                            if (sub.TimeStart>dial.TimeStart)
                                sub.TimeStart = dial.TimeStart;
                            if (sub.TimeEnd<dial.TimeEnd)
                                sub.TimeEnd = dial.TimeEnd;
                        }
                        else {
                            sub.TimeStart = dial.TimeStart;
                            sub.TimeEnd = dial.TimeEnd;
                        }
                        sub.Dialogues.push_back(dial);
                    }
                }
            }
        }
    }
//  else if (format==FORMAT_XXX) { // more to come...
//  }
    else {
        return false;
    }

    int const total_dialogues = (int) sub.Dialogues.size();
    if (total_dialogues>0) {
        sub.Dialogues.sort();
        sub.Format = format;

        // this, sadly, is trying to fix Big5 low byte conflicting with ASCII.
        if (CP_UTF8!=sub.Codepage) {
            BL_LOG("change Codepage:%d to UTF8(%d)\n", sub.Codepage, CP_UTF8);

            int max_len = 0;
            int max_wlen = 0;
            int utf8_len = 0;

            int const wtext_default_capacity = 2048;
            int wtext_capacity = wtext_default_capacity;
            wchar_t wtext_buffer_stack[wtext_default_capacity];
            wchar_t* wtext_buffer = wtext_buffer_stack;

            for (int i=0; i<total_dialogues; ++i) {
                SubStream::Dialogue const& d = sub.Dialogues[i];

                int src_length = int(d.TextEnd-d.TextStart);
                int len = MultiByteToWideChar(sub.Codepage, 0, d.TextStart, src_length, wtext_buffer, wtext_capacity);
                if (len>=wtext_capacity) {
                    len = MultiByteToWideChar(sub.Codepage, 0, d.TextStart, src_length, NULL, 0);
                    int new_capacity = wtext_capacity<<1;
                    while (new_capacity<len) {
                        new_capacity <<= 1;
                    }

                    if (wtext_buffer_stack!=wtext_buffer) {
                        free(wtext_buffer);
                    }

                    wtext_buffer = (wchar_t*) malloc(new_capacity*sizeof(wchar_t));
                    if (NULL==wtext_buffer) {
                        sub.Reset();
                        return false;
                    }

                    wtext_capacity = new_capacity;
                    len = MultiByteToWideChar(sub.Codepage, 0, d.TextStart, src_length, wtext_buffer, wtext_capacity);
                    if (len>=wtext_capacity) {
                        sub.Reset();
                        return false;
                    }
                }

                wtext_buffer[len] = 0;
                if (max_wlen<len)
                    max_wlen = len;

                len = WideCharToMultiByte(CP_UTF8, 0, wtext_buffer, len, NULL, 0, NULL, 0) + 1;
                if (max_len < len)
                    max_len = len;

                utf8_len += len;
            }

            int const alloc_size = (sub.StyleLength + utf8_len + 128) & ~127;
            uint8* const utf8 = (uint8*) malloc(alloc_size);
            char* ptr = (char*) utf8;
            char* const ptr_end = ptr + alloc_size;

            if (format==FORMAT_ASS) {
                memcpy(ptr, sub.Buffer, sub.StyleLength); ptr += sub.StyleLength;
                *ptr++ = '\0';
            }

            bool convert_failed = false;
            for (int i=0; i<total_dialogues&&!convert_failed; ++i) {
                SubStream::Dialogue& d = sub.Dialogues[i];
                int src_length = int(d.TextEnd-d.TextStart);
                int len = MultiByteToWideChar(sub.Codepage, 0, d.TextStart, src_length, wtext_buffer, wtext_capacity);
                if (0<len && len<wtext_capacity) {
                    int const capacity = (int) (ptr_end-ptr);
                    len = WideCharToMultiByte(CP_UTF8, 0, wtext_buffer, len, ptr, capacity, NULL, 0);
                    if (0<len && len<capacity) {
                        d.TextStart = ptr;
                        d.TextEnd = ptr += len;
                        *ptr++ = '\0';
                    }
                    else {
                        convert_failed = true;
                    }
                }
                else {
                    convert_failed = true;
                }
            }

            if (wtext_buffer_stack!=wtext_buffer) {
                free(wtext_buffer);
            }

            if (!convert_failed) {
                free(sub.Buffer);
                sub.Buffer = utf8;
                sub.BufferSize = alloc_size;
            }
            else {
                free(utf8);
                sub.Reset();
                return false;
            }
        }

        return true;
    }

    return false;
}
//---------------------------------------------------------------------------------------
bool Subtitle::AddSubtitleStream_(wchar_t const* filename, FORMAT format)
{
    if (NULL==filename || FORMAT_UNKNOWN==format ||
        totalExtSubtitleStreams_>=MAX_EXTERNAL_SUBTITLE_STREAMS) {
        return false;
    }

    //FILE* file = _wfopen(filename, L"rt,ccs=UNICODE"); // text mode or binary? uncode?
    //FILE* file = _wfopen(filename, L"rt,ccs=UTF-8");
    FILE* file = _wfopen(filename, L"rb");
    if (NULL==file) {
        return false;
    }

    SubStream& sub = extSubtitleStreams_[totalExtSubtitleStreams_];
    sub.Reset(); // just in case

    fseek(file, 0, SEEK_END);
    size_t const file_size = ftell(file);
    rewind(file);

    uint8 BOM[4];
    if (file_size<16 || 4!=fread(BOM, 1, 4, file)) {
        fclose(file);
        return false;
    }

    // 4 bytes extra for 0 (utf-32) terminated.
    sub.Buffer = (uint8*) malloc(file_size+4); 
    if (NULL==sub.Buffer) {
        fclose(file);
        return false;
    }

    uint8* ptr = sub.Buffer;

    //
    // check if BOM, Byte order mark
    //
    // Byte order mark  Description
    //  EF BB BF         UTF-8 
    //  FF FE            UTF-16 little endian 
    //  FE FF            UTF-16 big endian 
    //  FF FE 00 00      UTF-32 little endian 
    //  00 00 FE FF      UTF-32 big-endian 
    //
    sub.BufferSize = (int) file_size;
    int utf_chars = 0; // 1, 2, 4
    bool big_endian = false;
    if (0xEF==BOM[0] && 0xBB==BOM[1] && 0xBF==BOM[2]) {
        // UTF-8
        *ptr++ = BOM[3];
        utf_chars = 1;
        sub.BufferSize -= 3;
    }
    else if (0xFF==BOM[0] && 0xFE==BOM[1]) {
        if (0x00==BOM[2] && 0x00==BOM[3]) {
            // UTF-32 little endian 
            utf_chars = 4;
            sub.BufferSize -= 4;
        }
        else {
            // UTF-16 little endian
            *ptr++ = BOM[2];
            *ptr++ = BOM[3];
            sub.BufferSize -= 2;
            utf_chars = 2;
        }
    }
    else if (0xFE==BOM[0] && 0xFF==BOM[1]) {
        // UTF-16 big endian
        *ptr++ = BOM[2];
        *ptr++ = BOM[3];
        utf_chars = 2;
        sub.BufferSize -= 2;
        big_endian = true;
    }
    else if (0x00==BOM[0] && 0x00==BOM[1] && 0xFE==BOM[2] && 0xFF==BOM[3]) {
        // UTF-32 big-endian
        utf_chars = 4;
        sub.BufferSize -= 4;
        big_endian = true;
    }
    else {
        *ptr++ = BOM[0];
        *ptr++ = BOM[1];
        *ptr++ = BOM[2];
        *ptr++ = BOM[3];
    }

    size_t const remain_size = file_size - 4;
    if (remain_size!=fread(ptr, 1, remain_size, file)) {
        fclose(file);
        return false;
    }
    fclose(file); file = NULL;
    memset(ptr+remain_size, 0, 4); // 0 terminated

    sub.Codepage = 65001; // utf-8
    if (0==utf_chars) {
        int const detect_chars = (sub.BufferSize>1024) ? 1024:sub.BufferSize;
        sub.Codepage = 0;
        uchardet_t chardet = uchardet_new();
        if (0==uchardet_handle_data(chardet, (char const*) sub.Buffer, detect_chars)) {
            uchardet_data_end(chardet);
            const char* result = uchardet_get_charset(chardet);
            sub.Codepage = FindCodePage(result);
            if (65001==sub.Codepage) {
                utf_chars = 1;
            }
            else if (1200==sub.Codepage||1201==sub.Codepage) {
                utf_chars = 2;
                big_endian = (1201==sub.Codepage);
            }
            else if (12000==sub.Codepage||12001==sub.Codepage) {
                utf_chars = 4;
                big_endian = (12001==sub.Codepage);
            }
            BL_LOG("no BOM founds, detected result=%s Codepage: %u\n", result, sub.Codepage);
        }
        uchardet_delete(chardet);

        if (0==sub.Codepage) {
            BL_ERR("ucharset Handle error. assume to be utf-8\n");
            sub.Codepage = 65001;
            utf_chars = 1;
        }
    }

    if (0!=utf_chars) {
        if (2==utf_chars) {
            sub.BufferSize = sub.BufferSize & ~1;
            if (big_endian) {
                uint8* ptr = sub.Buffer;
                uint8 t;
                for (int i=0; i<sub.BufferSize; i+=2) {
                    t = ptr[0];
                    ptr[0] = ptr[1];
                    ptr[1] = t;
                    ptr += 2;
                }
            }

            // translate to utf8
            int len = WideCharToMultiByte(CP_UTF8, 0, (wchar_t const*) sub.Buffer, sub.BufferSize/2, NULL, 0, NULL, NULL);
            if (len>0) {
                uint8* utf8 = (uint8*) malloc(len+1);
                int len2 = WideCharToMultiByte(CP_UTF8, 0, (wchar_t const*) sub.Buffer, sub.BufferSize/2, (char*) utf8, len, NULL, NULL);
                if (len==len2) {
                    utf8[len] = '\0';
                    free(sub.Buffer);
                    sub.Buffer = utf8;
                    sub.BufferSize = len;
                }
                else {
                    free(utf8);
                    sub.Reset();
                    return false;
                }
            }
        }
        else if (4==utf_chars) {
            // !!!!!!!!!!!!!!!!!!!!!!!
            // have not checked yet!!!
            // !!!!!!!!!!!!!!!!!!!!!!!
            sub.BufferSize = sub.BufferSize & ~3;

            // translate to utf8, or failed.
            uint8* utf8 = NULL;
            int    utf8_len = 0;
            uint8 const* ptr = sub.Buffer;
            uint8 const* const ptr_end = sub.Buffer + sub.BufferSize;
            if (big_endian) {
                struct UTF32BE {
                    static uint32 cp(uint8 const* ptr) {
                        return (uint32(ptr[0])<<24) | (uint32(ptr[1])<<16) | (uint32(ptr[2])<<8) | uint32(ptr[3]);
                    }
                };
                int const len = GetUTF32toUTF8Size<UTF32BE>(ptr, ptr_end);
                if (len>0 && ptr==ptr_end) {
                    uint8* utf8 = (uint8*) malloc(len+1);
                    if (NULL!=utf8) {
                        ptr = sub.Buffer;
                        uint8* dst = utf8;
                        uint8* const dst_end = dst + len;
                        int const len2 = ConvertUTF32toUTF8<UTF32BE>(dst, dst_end, ptr, ptr_end);
                        if (len2==len && dst==dst_end && ptr==ptr_end) {
                            utf8[len] = '\0';
                            utf8_len = len;
                        }
                        else {
                            free(utf8);
                            utf8 = NULL;
                        }
                    }
                }
            }
            else {
                struct UTF32LE {
                    static uint32 cp(uint8 const* ptr) {
                        return (uint32(ptr[3])<<24) | (uint32(ptr[2])<<16) | (uint32(ptr[1])<<8) | uint32(ptr[0]);
                    }
                };
                int const len = GetUTF32toUTF8Size<UTF32LE>(ptr, ptr_end);
                if (len>0 && ptr==ptr_end) {
                    uint8* utf8 = (uint8*) malloc(len+1);
                    if (NULL!=utf8) {
                        ptr = sub.Buffer;
                        uint8* dst = utf8;
                        uint8* const dst_end = dst + len;
                        int const len2 = ConvertUTF32toUTF8<UTF32LE>(dst, dst_end, ptr, ptr_end);
                        if (len2==len && dst==dst_end && ptr==ptr_end) {
                            utf8[len] = '\0';
                            utf8_len = len;
                        }
                        else {
                            free(utf8);
                            utf8 = NULL;
                        }
                    }
                }
            }

            if (NULL!=utf8) {
                free(sub.Buffer);
                sub.Buffer = utf8;
                sub.BufferSize = utf8_len;
            }
            else {
                sub.Reset();
                return false;
            }
        }
        sub.Codepage = 65001;
    }

    sub.Format = FORMAT_UNKNOWN;
    if (ParseSubtitleStream_(sub, format)) {
        wchar_t wtext[1024];
#if 0
        for (int i=0; i<(int)sub.Dialogues.size(); ++i) {
            SubStream::Dialogue const& d = sub.Dialogues[i];

            int ds1 = (d.TimeStart%1000);
            int s1 = d.TimeStart/1000;
            int h1 = s1/3600; s1%=3600;
            int m1 = s1/60; s1%=60;

            int ds2 = (d.TimeEnd%1000);
            int s2 = d.TimeEnd/1000;
            int h2 = s2/3600; s2%=3600;
            int m2 = s2/60; s2%=60;

            int len = MultiByteToWideChar(CP_UTF8, 0, d.TextStart, (int) (d.TextEnd-d.TextStart), wtext, 1024);
            wtext[len] = 0;
            if (sub.Format==FORMAT_ASS) {
                BL_LOG("%01d:%02d:%02d.%02d, %01d:%02d:%02d.%02d ",
                        h1, m1, s1, ds1/10, h2, m2, s2, ds2/10);
                OutputDebugStringW(wtext);
                BL_LOG("\n");
            }
            else {
                BL_LOG("%d\n%02d:%02d:%02d,%03d --> %02d:%02d:%02d,%03d\n",
                        i+1, h1, m1, s1, ds1, h2, m2, s2, ds2);
                OutputDebugStringW(wtext);
                BL_LOG("\n\n");
            }
        }
#endif
        //
        // determine language, either by sub.Codepage or other libraries
        //
        if (ISO_639_UNKNOWN==sub.Language) {
            //
            // TO-DO : should rip out ASS control characters {\c}...
            //
            char const* const sample_text = sub.Dialogues[0].TextStart;
            int const sample_text_len = (int) (sub.Dialogues[0].TextEnd-sub.Dialogues[0].TextStart);
            assert(sample_text_len>0);
            
            if (CP_UTF8==sub.Codepage) {
#ifdef USE_GOOGLE_COMPACT_LANGUAGE_DETECTOR
                bool is_reliable = false;
#if 0
                CLD2::Language language3[3]; int percent3[3]; int text_bytes = 0;
                CLD2::Language const lang = CLD2::DetectLanguageSummary(sample_text,
                                                                     sample_text_len,
                                                                     true, // plain text
                                                                     language3,
                                                                     percent3,
                                                                     &text_bytes,
                                                                     &is_reliable);
#else
                CLD2::Language const lang = CLD2::DetectLanguage(sample_text,
                                                                 sample_text_len,
                                                                 true, // plain text
                                                                 &is_reliable);
#endif
                if (is_reliable) {
                    switch (lang)
                    {
                    case CLD2::ARABIC: sub.Language = ISO_639_ARA; break;
                    case CLD2::ENGLISH: sub.Language = ISO_639_ENG; break;
                    case CLD2::DANISH: sub.Language = ISO_639_DAN; break;
                    case CLD2::DUTCH: sub.Language = ISO_639_NLD; break;
                    case CLD2::GERMAN: sub.Language = ISO_639_DEU; break;
                    case CLD2::SPANISH: sub.Language = ISO_639_SPA; break;
                    case CLD2::PORTUGUESE: sub.Language = ISO_639_POR; break;
                    case CLD2::RUSSIAN: sub.Language = ISO_639_RUS; break;
                    case CLD2::INDONESIAN: sub.Language = ISO_639_IND; break;
                    case CLD2::TURKISH: sub.Language = ISO_639_TUR; break;
                    case CLD2::ICELANDIC: sub.Language = ISO_639_ISL; break;
                    case CLD2::FRENCH: sub.Language = ISO_639_FRA; break;
                    case CLD2::POLISH: sub.Language = ISO_639_POL; break;
                    case CLD2::FINNISH: sub.Language = ISO_639_FIN; break;
                    case CLD2::CZECH: sub.Language = ISO_639_CES; break;
                    case CLD2::SWEDISH: sub.Language = ISO_639_SWE; break;
                    case CLD2::ITALIAN: sub.Language = ISO_639_ITA; break;
                    case CLD2::GREEK: sub.Language = ISO_639_GRE; break;
                    case CLD2::JAPANESE: sub.Language = ISO_639_JPN; break;
                    case CLD2::KOREAN: sub.Language = ISO_639_KOR; break;
                    case CLD2::TAGALOG: sub.Language = ISO_639_TGL; break; // tagalog(filipino/philipine)
                    case CLD2::THAI: sub.Language = ISO_639_THA; break;
                    case CLD2::VIETNAMESE: sub.Language = ISO_639_VIE; break;
                    case CLD2::MALAY: sub.Language = ISO_639_MSA; break;

                    case CLD2::HINDI:
                    case CLD2::MALAYALAM:
                        sub.Language = ISO_639_HIN; break;

                    case CLD2::NORWEGIAN: // no
                    case CLD2::NORWEGIAN_N: // nn
                        sub.Language = ISO_639_NOR;
                        break;

                    case CLD2::CHINESE: // zh
                    case CLD2::CHINESE_T: // zh-Hant
                         sub.Language = ISO_639_CHI;
                         break;
                        
                    default:
                        break;
                    }
                }
#endif
            }
            else {
                sub.Language = CodePage_To_ISO_639(sub.Codepage);
            }

            if (ISO_639_UNKNOWN==sub.Language) {
                int len = MultiByteToWideChar(sub.Codepage, 0, sample_text, sample_text_len, wtext, 1024);
                wtext[len] = 0;

                MAPPING_ENUM_OPTIONS EnumOptions;
                memset(&EnumOptions, 0, sizeof(EnumOptions));
                EnumOptions.Size = sizeof(EnumOptions);
                EnumOptions.pGuid = const_cast<GUID*>(&ELS_GUID_LANGUAGE_DETECTION);

                DWORD dwServicesCount = 0;
                MAPPING_SERVICE_INFO* pService = NULL;
                HRESULT hr = MappingGetServices(&EnumOptions, &pService, &dwServicesCount);
                if (SUCCEEDED(hr)) {
                    MAPPING_PROPERTY_BAG bag;
                    memset(&bag, 0 , sizeof(bag));
                    bag.Size = sizeof(bag);
                    HRESULT hr = MappingRecognizeText(pService, wtext, len, 0, nullptr, &bag);
                    if (SUCCEEDED(hr) && bag.prgResultRanges && bag.prgResultRanges[0].pData) {
                        //BL_LOG("Service: %ws, category: %ws\n", pService[0].pszDescription, pService[0].pszCategory);
                        // Service: Microsoft Language Detection, category: Language Detection
                        // http://www.lingoes.net/en/translator/langcode.htm
                        //
                        // too bad it's not relible...
                        //
                        ISO_639 language[16];
                        int results(0), suspicious(0);
                        int bestguess = -1;
                        for (wchar_t const* lan = (wchar_t*) bag.prgResultRanges[0].pData;
                             lan[0]&&lan[1]&&results<16; ++lan) {
                            //BL_LOG("%ws\n", lan); // note :%ws only works for wide-character ANSI
                            ISO_639& res = language[results];
                            if (L'z'==lan[0] && L'h'==lan[1]) {
                                res = ISO_639_CHI;
                                if (bestguess<0)
                                    bestguess = results;
                                ++results;
                            }
                            else if (L'a'==lan[0] && L'r'==lan[1]) {
                                res = ISO_639_ARA;
                                if (bestguess<0)
                                    bestguess = results;
                                ++results;
                            }
                            else if (L'j'==lan[0] && L'a'==lan[1]) {
                                res = ISO_639_JPN;
                                if (bestguess<0)
                                    bestguess = results;
                                ++results;
                            }
                            else if (L'n'==lan[0] && L'l'==lan[1]) {
                                res = ISO_639_NLD;
                                if (bestguess<0)
                                    bestguess = results;
                                ++results;
                            }
                            else if (L'd'==lan[0] && L'e'==lan[1]) {
                                res = ISO_639_DEU;
                                if (bestguess<0)
                                    bestguess = results;
                                ++results;
                            }
                            else if (L'e'==lan[0] && L'n'==lan[1]) {
                                res = ISO_639_ENG;
                                if (L'-'==lan[2] && bestguess<0)
                                    bestguess = results;
                                ++results;
                            }
                            else if (L'e'==lan[0] && L'l'==lan[1]) {
                                res = ISO_639_GRE;
                                ++results;
                            }
                            else if (L'p'==lan[0] && L't'==lan[1]) {
                                res = ISO_639_POR;
                                ++results;
                            }
                            else {
                                BL_LOG("Could \"");
                                OutputDebugStringW(wtext);
                                BL_LOG("\" be %ws!?\n", lan);
                                ++suspicious;
                            }

                            for (lan += (len=2); len<16&&L'\0'!=*lan; ++lan) {
                                if (++len>16)
                                    break;
                            }

                            if (len>16 || L'\0'!=*lan)
                                break;
                        }

                        if (results>0) {
                            if (0<=bestguess && bestguess<results) {
                                sub.Language = language[bestguess];
                            }
                            else {
                                if (results<2 && results>suspicious) {
                                    sub.Language = language[0];
                                }
                            }
                        }
                        MappingFreePropertyBag(&bag);
                    } 
                    MappingFreeServices(pService);
                }
            }
        }
        ++totalExtSubtitleStreams_;
        return true;
    }
    else {
        sub.Reset();
        return false;
    }
}
//---------------------------------------------------------------------------------------
bool Subtitle::ASS_Dialogue_(SubStream::Dialogue& dialogue, char const* dialog,
                             uint32 const* dialogue_fields, int total_dialogue_fields) const
{
    assert(10==total_dialogue_fields);
    dialogue.Reset();

    char const* s = dialog;
    int const text_field_id = total_dialogue_fields - 1;
    for (int i=0; '\0'!=*s && i<total_dialogue_fields; ++i) {
        while (' '==*s) ++s;
        char const* e = s;
        while ((*e!=',' || (i==text_field_id)) && *e!='\0') {
            ++e;
        }

        uint32 const& field = dialogue_fields[i];
        if (s<e) {
            if (Dialogue_Start==field || Dialogue_End==field) {
                int time = -1;
                if ((s+7)<e && ':'==s[1] && ':'==s[4] && (':'==s[7] || ','==s[7] || '.'==s[7])) {
                    int hh = s[0] - '0';
                    int m1 = s[2] - '0';
                    int m2 = s[3] - '0';
                    int s1 = s[5] - '0';
                    int s2 = s[6] - '0';
                    int ts1 = ((s+8)<e) ? (s[8] - '0'):0;
                    int ts2 = ((s+9)<e) ? (s[9] - '0'):0;
                    if (0<=hh && hh<10 &&
                        0<=m1 && m1<10 &&
                        0<=m2 && m2<10 &&
                        0<=s1 && s1<10 &&
                        0<=s2 && s2<10 &&
                        0<=ts1 && ts1<10 &&
                        0<=ts2 && ts2<10) {
                        time = (hh*3600 + m1*600 + m2*60 + s1*10 + s2)*1000 + ts1*100 + ts2*10;
                    }
                }

                if (time>=0) {
                    if (Dialogue_Start==field) {
                        dialogue.TimeStart = time;
                    }
                    else {
                        dialogue.TimeEnd = time;
                    }
                }
            }
            else if (Dialogue_Style==field && totalStyles_>0) {
                dialogue.Style = mlabs::balai::CalcCRC((uint8 const*)s, (int)(e-s));
                if (Dialogue_Style_Default==dialogue.Style || Dialogue_Style_StarDefault==dialogue.Style) {
                    dialogue.Style = 0;
                }
            }
            else if (Dialogue_MarginL==field) {
                dialogue.MarginL = atol(s);
            }
            else if (Dialogue_MarginR==field) {
                dialogue.MarginR = atol(s);
            }
            else if (Dialogue_MarginV==field) {
                dialogue.MarginV = atol(s);
            }
            else if (Dialogue_Text==field) {
                assert(text_field_id==i);
                assert('\0'==*e);
                dialogue.TextStart = s;
                dialogue.TextEnd = e;
                break;
            }
            else if (Dialogue_Layer==field) {
                dialogue.Layer = atol(s);
            }
        }
        else {
            assert(Dialogue_Effect==field||Dialogue_Name==field);
        }

        s = ('\0'!=*e) ? (e+1):e;
    }

    return 0<=dialogue.TimeStart && dialogue.TimeStart<dialogue.TimeEnd &&
           NULL!=dialogue.TextStart && dialogue.TextStart<dialogue.TextEnd;
}
//---------------------------------------------------------------------------------------
inline bool line_break_character(wchar_t c) {
    // reference:
    // 0) http://www.unicode.org/charts/PDF/U0000.pdf C0 Controls and Basic Latin or ASCII table
    // 1) http://www.unicode.org/charts/PDF/U2000.pdf General Punctuation
    // 2) http://www.unicode.org/charts/PDF/U3000.pdf CJK Symbols and Punctuation
    // 3) http://www.unicode.org/charts/PDF/UFF00.pdf Halfwidth and Fullwidth Forms
    //
    // TO-DO : finish it...
    //
    return L'"'==c || 0x3003==c || 0x2033==c || 0xff02==c ||
           L','==c || 0xff0c==c || 0xff64==c || 0x3001==c || // IDEOGRAPHIC COMMA
           L'.'==c || 0xff0e==c || 0xff61==c || 0x3002==c || // IDEOGRAPHIC FULL STOP
           L'+'==c || 0xff0b==c ||
           L'('==c || 0xff08==c ||
           L')'==c || 0xff09==c ||
           L' '==c || 0x3000==c;
}
bool Subtitle::DialogueLineBreaker_(int& texture_width, int& texture_height, int& display_left,
                                    int& text_length, int& text_linebreaks,
                                    wchar_t const* ptr, wchar_t const* end,
                                    int display_height, int max_display_width)
{
    //
    // TO-DO : if wrapMode_=2, it should not break the line(only \n \N breaks)
    //
    text_length = text_linebreaks = texture_width = texture_height = 0;
    int const length = int(end - ptr);
    if (NULL!=ptr && length>0 &&
        textBlitter_.GetTextExtend(texture_width, texture_height, TEXT_FONT_PYRAMID, false, ptr, length) &&
        texture_width>0 && texture_height>0) {
        text_length = length;
        int display_width = display_height*texture_width/texture_height;
        if (display_left+display_width>max_display_width) {
            int const display_width_full = display_width;
            int const texture_width_full = texture_width;
            int const texture_height_full = texture_height;

            // use 'space' character to break this paragraph...
            text_length = length - 1;
            while (text_length>0) {
                if ((L' '==ptr[text_length] || 0x3000==ptr[text_length]) &&
                    textBlitter_.GetTextExtend(texture_width, texture_height, TEXT_FONT_PYRAMID, false, ptr, text_length) &&
                    texture_width>0 && texture_height>0 &&
                    (display_left+(display_width=(display_height*texture_width/texture_height)))<max_display_width) {
                    if (100*(display_left+display_width)>60*max_display_width) {
                        return true;
                    }
                    break;
                }
                --text_length;
            }

            // no 'space' characters to break, make more room...
            if (100*display_left>60*max_display_width) { // a line already 60% capacity
                display_left = 0;
                text_linebreaks = 2; // newline before paragraph start

                // no need to break!?
                if (display_width_full<max_display_width) {
                    texture_width = texture_width_full;
                    texture_height = texture_height_full;
                    text_length = length;
                    return true;
                }
            }

            // break again
            text_length = length - 1;
            while (text_length>0) {
                if (line_break_character(ptr[text_length]) && 
                    textBlitter_.GetTextExtend(texture_width, texture_height, TEXT_FONT_PYRAMID, false, ptr, text_length+1) &&
                    texture_width>0 && texture_height>0 &&
                    (display_left+(display_width=(display_height*texture_width/texture_height)))<max_display_width) {
                    ++text_length; // + 1
                    return true;
                }
                --text_length;
            }
/*

FIX-ME!!! It's not correct!!!

            // and again...
            for (int i=0; i<length; ++i) {
                if ((L' '==ptr[i] || 0x3000==ptr[i]) &&
                    textBlitter_.GetTextExtend(texture_width, texture_height, TEXT_FONT_PYRAMID, false, ptr, i) &&
                    texture_width>0 && texture_height>0) {
                    display_width = display_height*texture_width/texture_height;
                    text_length = i + 1;

                    text_linebreaks |= 1; // newline after paragraph end

                    return true;
                }
            }
*/
            // give up, no linebreaks
            texture_width = texture_width_full;
            texture_height = texture_height_full;
            text_length = length;

            if (0==text_linebreaks || (100*display_width_full)>(80*max_display_width)) {
                text_linebreaks |= 1; // newline after paragraph end
            }
        } // force a linebrake

        return true;
    }

    return false;
}
int Subtitle::SRT_Dialogue_Text_(uint8* buffer, int buffer_size, int& width, int& height,
                                 SubtitleRect* rects, int max_rects,
                                 SubStream::Dialogue const* dialogues, int totals,
                                 int timestamp, int charset)
{
    assert(NULL!=dialogues && totals>0 && max_rects>0);

    //
    // html font size table(default size=3)
    //  <font size=     '1',   '2',   '3',   '4',   '5',   '6',   '7'
    //  Safari/Chrome  10px,  13px,  16px,  18px,  24px,  32px,  48px
    //  Firefox        10px,  13px,  15px,  18px,  23px,  30px,  45px
    //
    // map html font size to pixel
    int const size_table[] = { 0, 10, 13, 16, 18, 24, 32, 48 };
    int const default_html_font_size = size_table[3];

    // prevent over estimate for full frame SBS 3D
    float const aspect_ratio = (float)playResX_/(float)playResY_;
    int const max_subtitle_display_width = (aspect_ratio>2.5f) ? (playResX_*49/100): (playResX_*95/100);
    int const margin_bottom = playResY_*990/1000;

    FontParam font_default, current, desired;
    styles_[0].BuildFontParam(font_default);
    desired = current = font_default;

    int const total_paragraph_stack = 32;
    Color font_color_stack[total_paragraph_stack];
    char const* font_face_stack[total_paragraph_stack];
    char const* font_face_end_stack[total_paragraph_stack];
    int font_size_stack[total_paragraph_stack];

    struct Paragraph {
        FontParam font;
        Color color;
        char* text;
        int   length;
        int   size;
    } paragraphs[MAX_NUM_SUBTITLE_RECTS];

    struct TextGlyph {
        FontParam font;
        wchar_t* text;
        int length;
        int display_top; // offset from DialogueRect::top
        int display_width, display_height;
        int texture_width_in, texture_height_in;
        int texture_width_out, texture_height_out;
    } textGlyphes[MAX_NUM_SUBTITLE_RECTS];

    struct DialogueRect {
        TextGlyph* glyphes;
        SubtitleRect* rects;
        int num_rects;
        int top, width, height;
        int time_start, time_end;
    } dialogueRects[MAX_NUM_SUBTITLE_RECTS];
    int num_dialogueRects = 0;

    // text buffer - buffer for 'a' dialgue.
    int const text_buffer_size = 1024;
    char text_buffer[text_buffer_size];
    char* const text_buffer_end = text_buffer + text_buffer_size;

    // wide character text buffer for all dialogues 
    int const wtext_buffer_size = 4096;
    wchar_t wtext_buffer[wtext_buffer_size];
    wchar_t* const wtext_buffer_end = wtext_buffer + wtext_buffer_size;
    wchar_t* wptr = wtext_buffer; *wptr = L'\0';

    int num_rects = width = height = 0;
    for (int dialId=0; dialId<totals; ++dialId) {
        SubStream::Dialogue const& dialogue = dialogues[dialId];
        if (timestamp>=dialogue.TimeEnd)
            continue;

        int num_paragraphs = 0;
        char* ptr = text_buffer;
        Paragraph* pg = paragraphs;
        pg->font = desired = font_default;
        pg->color = font_color_stack[0] = styles_[0].PrimaryColour;
        pg->text = ptr; *ptr = '\0';
        pg->length = 0;
        pg->size = font_size_stack[0] = 3;

        font_face_stack[0] = font_default.name;
        font_face_end_stack[0] = NULL;
        int font_stack = 0;
        int bold_stack = 0;
        int italic_stack = 0;
        int underline_stack = 0;
        int strikeout_stack = 0;

        char const* s = dialogue.TextStart;
        while (s<dialogue.TextEnd && ptr<text_buffer_end) {
            if (*s=='<' || *s=='{') {
                char const dlimit = (*s=='<') ? '>':'}';
                bool const pop = (*++s=='/');
                if (pop) {
                    ++s;
                }

                if (s[1]==dlimit) {
                    if (s[0]=='b') {
                        if (pop) {
                            --bold_stack;
                            assert(bold_stack>=0);
                            desired.bold = (bold_stack>0);
                        }
                        else {
                            ++bold_stack;
                            desired.bold = 1;
                        }
                    }
                    else if (s[0]=='i') {
                        if (pop) {
                            --italic_stack;
                            assert(italic_stack>=0);
                            desired.italic = italic_stack>0;
                        }
                        else {
                            ++italic_stack;
                            desired.italic = 1;
                        }
                    }
                    else if (s[0]=='u') {
                        if (pop) {
                            --underline_stack;
                            assert(underline_stack>=0);
                            desired.underline = (underline_stack>0);
                        }
                        else {
                            ++underline_stack;
                            desired.underline = 1;
                        }
                    }
                    else if (s[0]=='s') {
                        if (pop) {
                            --strikeout_stack;
                            assert(strikeout_stack>=0);
                            desired.strikeout = (strikeout_stack>0);
                        }
                        else {
                            ++strikeout_stack;
                            desired.strikeout = 1;
                        }
                    }
                    //else { uncognize tag }

                    ++s;
                }
                else if (s[0]=='f' && s[1]=='o' && s[2]=='n' && s[3]=='t') {
                    char const* e = s+=4;
                    char const* face = NULL;
                    char const* color = NULL;
                    char const* size = NULL;
                    while (*e!=dlimit && e<dialogue.TextEnd) {
                        if (*e=='=') {
                            if ((s+5)<=e) {
                                if (0==memcmp(e-4, "face", 4)) {
                                    face = e+=2;
                                }
                                else if (0==memcmp(e-4, "size", 4)) {
                                    size = e+=2;
                                }
                                else if (0==memcmp(e-5, "color", 5)) {
                                    color = e+=2;
                                }
                            }
                        }
                        ++e;
                    }

                    if (*e!=dlimit)
                        break;

                    if (pop) {
                        if (--font_stack<0) {
                            font_stack = total_paragraph_stack - 1; // NO!
                        }
                    }
                    else {
                        // current stack value
                        char const* fn = font_face_stack[font_stack];
                        char const* fn_end = font_face_end_stack[font_stack];
                        int fs = font_size_stack[font_stack];
                        Color fc = font_color_stack[font_stack];

                        if (++font_stack>total_paragraph_stack) {
                            font_stack = 0; // NO!
                        }

                        if (NULL!=face) {
                            s = face + 1;
                            while (*s!='\'' && *s!='\"' && s<e) {
                                ++s;
                            }

                            if (s<e) {
                                fn = face;
                                fn_end = s;
                            }
                        }

                        if (NULL!=size && isdigit(*size)) {
                            fs = atol(size);
                            if (fs<1)
                                fs = 1;
                            else if (fs>7)
                                fs = 7;
                        }

                        if (NULL!=color && color[0]=='#') {
                            fc.r = hex(color[1], color[2]);
                            fc.g = hex(color[3], color[4]);
                            fc.b = hex(color[5], color[6]);
                            fc.a = 255;
                        }

                        font_face_stack[font_stack] = fn;
                        font_face_end_stack[font_stack] = fn_end;
                        font_size_stack[font_stack] = fs;
                        font_color_stack[font_stack] = fc;
                    }

                    //
                    char const* fn = font_face_stack[font_stack];
                    char const* fn_end = font_face_end_stack[font_stack];
                    if (fn<fn_end && (fn+FontParam::MAX_NAME_LENGTH)>fn_end) {
                        int const len = (int)(fn_end-fn);
                        memcpy(desired.name, fn, len);
                        desired.name[len] = '\0';
                    }
                    else {
                        memcpy(desired.name, font_default.name, FontParam::MAX_NAME_LENGTH);
                    }

                    s = e;
                }
                else {
                    while (*s!=dlimit && s<dialogue.TextEnd) {
                        ++s;
                    }
                }

                if (pg->length>0) {
                    if (pg->color != font_color_stack[font_stack] ||
                        pg->font != desired ||
                        pg->size != font_size_stack[font_stack]) {

                        // new paragraph start
                        if (++num_paragraphs>=MAX_NUM_SUBTITLE_RECTS) {
                            break;
                        }

                        pg = paragraphs + num_paragraphs;
                        pg->text = ptr; *ptr = '\0';
                        pg->length = 0;
                    }
                }

                pg->font = desired;
                pg->color = font_color_stack[font_stack];
                pg->size = font_size_stack[font_stack];
            }
            else {
                *ptr++ = *s;
                ++(pg->length);
            }

            ++s;
        }

        if (pg->length>0) {
            ++num_paragraphs;
        }
        else if (num_paragraphs<=0) {
            continue;
        }

        // collect paragraphes of this dialogue...
        DialogueRect& dial_rect = dialogueRects[num_dialogueRects];
        dial_rect.glyphes = textGlyphes + num_rects;
        dial_rect.rects = rects + num_rects;
        dial_rect.num_rects = 0;
        dial_rect.top = dial_rect.width = dial_rect.height = 0;
        // be sure to clamp start time
        dial_rect.time_start = ((dialogue.TimeStart)<timestamp) ? timestamp:(dialogue.TimeStart);
        dial_rect.time_end = dialogue.TimeEnd;

        int display_left(0), display_top(0);
        int ww(0), hh(0), display_height_max(0);
        for (int i=0; i<num_paragraphs; ++i) {
            pg = paragraphs + i;

            if (!IsFontAvailable(pg->font, ISO_639_UNKNOWN, charset)) {
                memcpy(pg->font.name, font_default.name, FontParam::MAX_NAME_LENGTH);
            }

            if (pg->font!=current) {
                SetFont_(pg->font, charset);
                current = pg->font;
            }

            int const dist_border_size = textBlitter_.GetDistanceMapSafeBorder(TEXT_FONT_PYRAMID);
            int display_height = ((playResY_*size_table[pg->size]/default_html_font_size)*TEXT_DEFAULT_DISPLAY_HEIGHT_RATIO_PER_MILLI+500)/1000;
            if ((movieResX_*9)>(movieResY_*16))
                display_height = display_height*(movieResX_*9)/(movieResY_*16);

            char const* s = pg->text;
            char const* const text_end = pg->text + pg->length;
            while (s<text_end && num_rects<max_rects) {
                if (*s=='\n') {
                    display_left = 0;
                    if (display_height_max>0) {
                        display_top += display_height_max;
                        display_height_max = 0;
                    }
                    else {
                        display_top += display_height;
                    }
                    ++s;
                    continue;
                }

                char const* e = s;
                while (e<text_end && *e!='\n') ++e;
                wchar_t const* const wptr_end = wptr + MultiByteToWideChar(CP_UTF8, 0, s, (int) (e-s), wptr, (int)(wtext_buffer_end-wptr));
                if (wptr<wptr_end) {
                    int length(0), linebreaks(0);
                    while (wptr<wptr_end && num_rects<max_rects &&
                           DialogueLineBreaker_(ww, hh, display_left, length, linebreaks,
                                                wptr, wptr_end, display_height, max_subtitle_display_width)) {
                        if (0!=(linebreaks&2)) {
                            assert(0==display_left);
                            display_top += display_height_max;
                            display_height_max = 0;
                        }
                        TextGlyph& glyph = dial_rect.glyphes[dial_rect.num_rects];
                        glyph.font = pg->font;
                        glyph.text = wptr;
                        glyph.length = length;
                        glyph.display_top = display_top;
                        glyph.display_width = display_height*ww/hh;
                        glyph.display_height = display_height;

                        // texture size(over estimate)
                        glyph.texture_width_in = ww;
                        glyph.texture_height_in = hh;
                        glyph.texture_width_out = ww + 2*dist_border_size;
                        glyph.texture_height_out = hh + 2*dist_border_size;

                        if (width<glyph.texture_width_out)
                            width = glyph.texture_width_out;

                        if (display_height_max<display_height)
                            display_height_max = display_height;

                        display_left += glyph.display_width;
                        if (dial_rect.width<display_left) {
                            dial_rect.width = display_left;
                        }

                        SubtitleRect& rect = dial_rect.rects[dial_rect.num_rects];
                        rect.Reset();
                        rect.PrimaryColour = pg->color;
                        rect.TimeStart = dial_rect.time_start;
                        rect.TimeEnd  = dial_rect.time_end;
                        rect.PlayResX = (float) playResX_;
                        rect.PlayResY = (float) playResY_;

                        // to be determined...
                        //rect.X = rect.Y = rect.Width = rect.Height = 0;
                        //rect.s0 = rect.t0 = rect.s1 = rect.t1 = 0.0f;
                        ++dial_rect.num_rects;
                        ++num_rects;

                        if ((wptr+=length)<wptr_end || 0!=(linebreaks&1)) {
                            display_left = 0;
                            display_top += display_height_max;
                            display_height_max = 0;
                            while (wptr<wptr_end && (L' '==*wptr||0x3000==*wptr)) {
                                ++wptr;
                            }
                        }
                    }
                }

                // must respect this linebreak set by srt
                if (e<text_end && *e=='\n') {
                    display_left = 0;
                    display_top += display_height_max;
                    display_height_max = 0;
                }

                s = e + 1;
            }
        }

        if (dial_rect.num_rects>0) {
            ++num_dialogueRects;
            dial_rect.height = display_top + display_height_max;
            dial_rect.top = margin_bottom - dial_rect.height;
        }
    } // for each dialogue

    if (num_rects<0)
        return 0;

    // conservative determine width.
    width = (width + TEXT_FONT_SPACE_GUARDBAND + 3) & ~3;
    if (width<512) width = 512;

    // and height
    int hh(0), s0(0), t0(0);
    for (int i=0; i<num_rects; ++i) {
        TextGlyph& glyph = textGlyphes[i];
        if ((s0+=(glyph.texture_width_out+TEXT_FONT_SPACE_GUARDBAND))>width) {
            t0 += (hh + TEXT_FONT_SPACE_GUARDBAND);
            s0 = glyph.texture_width_out + TEXT_FONT_SPACE_GUARDBAND;
            hh = glyph.texture_height_out;
        }
        else if (hh<glyph.texture_height_out) {
            hh = glyph.texture_height_out;
        }
    }
    height = t0 + hh;// + TEXT_FONT_SPACE_GUARDBAND;

    int const required_size = width*height;
    if (required_size>buffer_size) {
        return -required_size;
    }

    // clear image
    memset(buffer, 0, required_size);

    s0 = t0 = hh = 0;
    for (int i=0; i<num_rects; ++i) {
        TextGlyph& glyph = textGlyphes[i];
        if ((s0+glyph.texture_width_out+ TEXT_FONT_SPACE_GUARDBAND)>width) {
            t0 += (hh + TEXT_FONT_SPACE_GUARDBAND);
            s0 = hh = 0;
        }

        if (glyph.font!=current) {
            SetFont_(glyph.font, charset);
            current = glyph.font;
        }

        textBlitter_.DistanceMap(buffer + width*t0 + s0,
                                 glyph.texture_width_out, glyph.texture_height_out,
                                 glyph.texture_width_in, glyph.texture_height_in,
                                 width, TEXT_FONT_PYRAMID, glyph.text, glyph.length);

        SubtitleRect& rect = rects[i];
        rect.Width = (float) glyph.display_width*glyph.texture_width_out/glyph.texture_width_in;
        rect.Height = (float) glyph.display_height*glyph.texture_height_out/glyph.texture_height_in;
        rect.s0 = (float)s0/(float)width;
        rect.s1 = (float)(s0+glyph.texture_width_out)/(float)width;
        rect.t0 = (float)t0/(float)height;
        rect.t1 = (float)(t0+glyph.texture_height_out)/(float)height;

        s0 += (glyph.texture_width_out + TEXT_FONT_SPACE_GUARDBAND);
        if (hh<glyph.texture_height_out)
            hh = glyph.texture_height_out;
    }
    assert((t0+hh)<=height);

    if (current!=font_default) {
        SetFont_(font_default, charset);
    }

    //
    // finally rect.X and rect.Y...
    for (int j=num_dialogueRects-1; j>0; --j) {
        DialogueRect const& dj = dialogueRects[j];
        for (int i=j-1; i>=0; --i) {
            DialogueRect& di = dialogueRects[i];
            if (di.time_start<=dj.time_start && dj.time_start<di.time_end) {
                hh = dj.top - di.height; 
                if (di.top>hh) di.top = hh;
            }
        }
    }

    float very_top = (float) playResY_;
    for (int i=0; i<num_dialogueRects; ++i) {
        DialogueRect& d = dialogueRects[i];
        for (int j=0; j<d.num_rects; ) {
            int k = j;
            int ww = hh = 0;
            int const display_top = d.glyphes[j].display_top;
            for (; j<d.num_rects && display_top==d.glyphes[j].display_top; ++j) {
                ww += d.glyphes[j].display_width;
                if (hh<d.glyphes[j].display_height)
                    hh = d.glyphes[j].display_height;
            }
            float const bottom = (float) (d.top + display_top + hh);

            ww = (playResX_-ww)/2;
            for (; k<j; ++k) {
                SubtitleRect& rect = d.rects[k];
                TextGlyph const& glyph = d.glyphes[k];
                rect.X = (float) ww - 0.5f*(rect.Width-glyph.display_width);
                rect.Y = bottom - 0.5f*(rect.Height+glyph.display_height) +
                              0.25f*(glyph.display_height-hh); // baseline align(roughly)
                ww += glyph.display_width;
                if (very_top>rect.Y)
                    very_top = rect.Y;
            }
        }
    }

    //
    // some subtitles are above screen, and the VLC play choose to
    // reposite the toppest subtitle to be aligned on screen edge, cancel
    // some subtitle below screen
    if (very_top<0) {
        int const num_rects_bak = num_rects;
        num_rects = 0;
        for (int i=0; i<num_rects_bak; ++i) {
            SubtitleRect& rect = rects[i];
            rect.Y -= very_top;
            if ((rect.Y+rect.Height)<=playResY_) {
                if (num_rects<i) {
                    memcpy(rects+num_rects, &rect, sizeof(SubtitleRect));
                }
                ++num_rects;
            }
        }
    }

    return num_rects;
}
//---------------------------------------------------------------------------------------
int Subtitle::ASS_Dialogue_Text_(uint8* buffer, int buffer_size, int& width, int& height,
                                 SubtitleRect* rects, int max_rects,
                                 SubStream::Dialogue const* dialogues, int totals,
                                 int timestamp, int charset)
{
    assert(NULL!=dialogues && totals>0 && max_rects>0);
    assert(0<=defaultStyle_ && defaultStyle_<totalStyles_);

    Style const& default_style = styles_[defaultStyle_];
    FontParam font_default, current, desired;
    default_style.BuildFontParam(font_default);
    current = font_default;

    //
    // if we have both playResX_, playResY_ set but the aspect ratio be huge different
    // from of movieResX_, movieResY_. it may go wrong seriously. must correct it!
    int corrected_PlayResX(playResY_*movieResX_/movieResY_), corrected_PlayResY(playResY_);
    //int corrected_PlayResX(playResX_), corrected_PlayResY(playResY_);

    int font_size_adj_num = 1000; // numerator
    int const font_size_adj_den = 1000; // denominator
    if (1==totalStyles_) {
        // subject to :
        // TEXT_DEFAULT_DISPLAY_HEIGHT_RATIO_PER_MILLI/1000 = default_style.Fontsize*font_size_adj_num/(playResY_*font_size_adj_den);
        font_size_adj_num = playResY_*font_size_adj_den*TEXT_DEFAULT_DISPLAY_HEIGHT_RATIO_PER_MILLI/(1000*default_style.Fontsize);
        if (font_size_adj_num<font_size_adj_den)
            font_size_adj_num = font_size_adj_den;
    }

    // prevent over estimate for full frame 3D
    float const aspect_ratio = (float)movieResX_/(float)movieResY_;
    if (aspect_ratio>2.5f) { // full frame SBS 3D
        corrected_PlayResX /= 2;
    }
    else if (aspect_ratio<1.0f) {
        // e.g. Blu-ray Full High Definition 3D (FHD3D) 1920x2205
        corrected_PlayResY /= 2;
    }

    // \fade
    struct FadeController {
        int   t1, t2, t3, t4;
        uint8 a1, a2, a3, enable;
        operator bool() {
            return 0!=(enable=(t1<=t2 && t2<=t3 && t3<=t4 && (t1<t2 || t3<t4)));
        }
        void Reset(int s=0, int e=0) {
            t1 = t2 = s; t3 = t4 = e;
            a1 = 0; a2 = 255; a3 = enable = 0;
        }
        bool Equals(FadeController const& b) const {
            if (enable) {
                if (b.enable) {
                    return t1==b.t1 && t2==b.t2 && t3==b.t3 && t4==b.t4 &&
                           a1==b.a1 && a2==b.a2 && a3==b.a3;
                }
                return false;
            }
            return (0==b.enable);
        }
    } fade;

    // animation key buffer for a dialogue
    struct AnimationSet {
        struct AnimationKey {
            int t1, t2, value;
            float accl;
        } Keys[SubtitleRect::ANIMATION_BUFFER_SIZE];
        struct Animation {
            int KeyIds[SubtitleRect::ANIMATION_BUFFER_SIZE];
            int nKeys;
            bool Equals(Animation const& that) const {
                if (nKeys==that.nKeys) {
                    for (int i=0; i<nKeys; ++i) {
                        if (KeyIds[i]!=that.KeyIds[i])
                            return false;
                    }
                    return true;
                }
                return false;
            }
            Animation():nKeys(0) {}
            Animation& operator=(Animation const& that) {
                if (this!=&that) {
                    nKeys = that.nKeys;
                    for (int i=0; i<nKeys; ++i) {
                        KeyIds[i] = that.KeyIds[i];
                    }
                }
                return *this;
            }
            operator bool() const { return nKeys>0; }
        };
        Animation fs, fr, fscx, fscy;
        int nKeys;

        void Clear_(Animation& anim) {
            if (anim.nKeys==nKeys) {
                // if takes out each keys, it must remap other animations indices.
                // Do NOT do it!
                nKeys = 0;
            }
            anim.nKeys = 0;

            //
            // but you can still reset nKeys, if all animations have no keys...
            if (0==fs.nKeys && 0==fr.nKeys && 0==fscx.nKeys && 0==fscy.nKeys
                /* more to come... */ ) {
                nKeys = 0;
            }
        }
        bool AddKey_(Animation& anim, int t1, int t2, float accl, int value) {
            if (nKeys<SubtitleRect::ANIMATION_BUFFER_SIZE) {
                anim.KeyIds[anim.nKeys++] = nKeys;
                AnimationKey& key = Keys[nKeys++];
                key.t1 = t1;
                key.t2 = t2;
                key.value = value;
                key.accl = accl;
                return true;
            }
            return false;
        }

        AnimationSet():nKeys(0) {}
        AnimationSet& operator=(AnimationSet const& that) {
            if (this!=&that) {
                nKeys = that.nKeys;
                for (int i=0; i<nKeys; ++i) {
                    Keys[i] = that.Keys[i];
                }
                fr = that.fr;
                fs = that.fs;
            }
            return *this;
        }
        operator bool() const { return nKeys>0; }
        void Reset() {
            fr.nKeys = fs.nKeys = nKeys = 0;
        }
        bool Equals(AnimationSet const& that) const {
            if (nKeys==that.nKeys && fs.Equals(that.fs) && fr.Equals(that.fr)) {
                for (int i=0; i<nKeys; ++i) {
                    AnimationKey const& a = Keys[i];
                    AnimationKey const& b = that.Keys[i];
                    if (a.accl!=b.accl || a.t1!=b.t1 || a.t2!=b.t2 ||a.value!=b.value)
                        return false;
                }
                return true;
            }
            return false;
        }

        bool t_fs(int t1, int t2, float accl, int value) {
            return AddKey_(fs, t1, t2, accl, value);
        }
        void t_fs_clear() { Clear_(fs); }

        bool t_fr(int t1, int t2, float accl, int value) {
            return AddKey_(fr, t1, t2, accl, value);
        }
        void t_fr_clear() { Clear_(fr); }

        bool t_fscx(int t1, int t2, float accl, int value) {
            return AddKey_(fscx, t1, t2, accl, value);
        }
        void t_fscx_clear() { Clear_(fscx); }

         bool t_fscy(int t1, int t2, float accl, int value) {
            return AddKey_(fscy, t1, t2, accl, value);
        }
        void t_fscy_clear() { Clear_(fscy); }

    } animSet;

    struct Paragraph {
        FontParam font;
        char* text;
        int   length;

        // ASS Style override codes.
        // refer Appendix A, Sub Station Alpha v4.0+ Script Format.

        // PrimaryColour, SecondaryColour, OutlineColour and BackColour respectively.
        Color color1, color2, color3, color4;

        // Animation set \t
        AnimationSet animSet;

        // \fad(t1,t2), \fade(a1,a2,a3,a4,t1,t2,t3,t4)
        FadeController fade;

        int   fs;          // \fs, font size
        int   fscx, fscy;  // \fscx, \fscy, scale x & y in percentage
        int   fe;          // \fe, encoding
        int   karaokeTime; // \k, \kf, \K, \ko, Karaoke duration
        int   karaokeSequence;

        // \frx, \fry, \frz (or \fr), rotate angle in degrees
        float frx, fry, frz;

        uint8 bord;        // \bord, border(outline) thickness, [0, 4]
        uint8 shad;        // \shad, shadow thickness, [0, 4]
        uint8 pMode;       // \p, drawing mode, currently not support(ignored)
        uint8 karaoke;
    } paragraphs[MAX_NUM_SUBTITLE_RECTS];

    struct TextGlyph {
        FontParam font;
        wchar_t* text;
        int length;
        int display_top;
        int display_width, display_height;
        int texture_width_in, texture_height_in;
        int texture_width_out, texture_height_out;
        int karaokeTime, karaokeSequence;
    } textGlyphes[MAX_NUM_SUBTITLE_RECTS];

    struct DialogueRect {
        TextGlyph* glyphes;
        SubtitleRect* rects;
        int num_rects;
        float left, top;
        int width, height;
        int time0, time_start, time_end;
        int moveT1, moveT2;
        int marginL, marginR, marginV;

        float moveX1, moveY1, moveX2, moveY2;
        float orgX, orgY; // \org() rotate origin, first appearance count

        uint8 alignment;
        uint8 moveCmd;
        uint8 tCmd;
        uint8 orgCmd;

        uint8 karaokeOn;
        uint8 rotateOn;
    } dialogueRects[MAX_NUM_SUBTITLE_RECTS];
    int num_dialogueRects = 0;

    // text buffer for a dialogue
    int const text_buffer_size = 1024;
    char text_buffer[text_buffer_size];
    char* const text_buffer_end = text_buffer + text_buffer_size;

    // wide text buffer
    int const wtext_buffer_size = 4096;
    wchar_t wtext_buffer[wtext_buffer_size];
    wchar_t* const wtext_buffer_end = wtext_buffer + wtext_buffer_size;
    wchar_t* wptr = wtext_buffer; *wptr = L'\0';
    int num_rects = width = height = 0;

    // t command(trying to support)
    uint8 const tCmd_on = 1;
    uint8 const tCmd_fs = 1<<1;
    uint8 const tCmd_fr = 1<<2;
    uint8 const tCmd_fscx = 1<<3;
    uint8 const tCmd_fscy = 1<<4;

    for (int dialId=0; dialId<totals; ++dialId) {
        SubStream::Dialogue const& dialogue = dialogues[dialId];
        if (timestamp>=dialogue.TimeEnd)
            continue;

        int styleId = defaultStyle_;
        if (dialogue.Style!=0) {
            for (int i=0; i<totalStyles_; ++i) {
                if (dialogue.Style==styles_[i].Name) {
                    styleId = i;
                    break;
                }
            }
        }
        Style const& style = styles_[styleId];
        style.BuildFontParam(desired);

        Color color1(style.PrimaryColour), color2(style.SecondaryColour),
              color3(style.OutlineColour), color4(style.BackColour);

        fade.Reset(dialogue.TimeStart, dialogue.TimeEnd);
        animSet.Reset();

        int fs = style.Fontsize;
        int fscx = style.ScaleX;
        int fscy = style.ScaleY;
        int fe = style.Encoding;
        int karaokeTime(0), moveT1(0), moveT2(0);
        int karaokeSequence = 0;
        int WrapMode = wrapMode_;
        float frx(0.0f), fry(0.0f), frz(style.Angle);

        float orgX(-1.0f), orgY(-1.0f);
        float moveX1(-1.0f), moveY1(-1.0f), moveX2(-1.0f), moveY2(-1.0f);

        uint8 Alignment = 0; // only the first appearance count
        uint8 bord_style = (3==style.BorderStyle) ? 0x80:0x00;
        uint8 bord = style.Outline;
        uint8 shad = style.Shadow;
        uint8 pMode = 0;
        uint8 karaoke = 0;

        // \t, \move, \pos (and \org check VLC player) will ignore collision detection
        uint8 moveCmd = 0;
        uint8 tCmd = 0;
        uint8 orgCmd = 0;

        char* ptr = text_buffer;
        int num_paragraphs = 0;

        Paragraph* pg = paragraphs;
        pg->font = desired;

        pg->text = ptr; *ptr = '\0';
        pg->length = 0;
        pg->color1 = color1;
        pg->color2 = color2;
        pg->color3 = color3;
        pg->color4 = color4;

        pg->fade.Reset(dialogue.TimeStart, dialogue.TimeEnd);
        pg->animSet.Reset();

        pg->fs = fs;
        pg->fscx = fscx;
        pg->fscy = fscy;
        pg->fe = fe;
        pg->frx = frx;
        pg->fry = fry;
        pg->frz = frz;

        pg->bord = (bord||shad) ? (bord_style|bord):0;
        pg->shad = shad;

        pg->pMode = pMode;

        pg->karaokeTime = karaokeTime;
        pg->karaoke = karaoke;
        pg->karaokeSequence = karaokeSequence;

        char const* s = dialogue.TextStart;
        char const* e = s;
        while (s<dialogue.TextEnd && num_paragraphs<MAX_NUM_SUBTITLE_RECTS && ptr<text_buffer_end) {
            if (*s=='{') {
                e = ++s;
                while (*e!='}' && e<dialogue.TextEnd) {
                    ++e;
                }

                if (*e!='}')
                    break;

                while (*s=='\\' && s<e) {
                    char const* e1 = ++s;
                    if (*s=='t') {
                        assert(s[1]=='(');
                        int parenthesis = 0;
                        while ((*e1!='\\' || parenthesis>0) && e1<e) {
                            if (*e1=='(') {
                                ++parenthesis;
                            }
                            else if (*e1==')') {
                                assert(parenthesis>0);
                                --parenthesis;
                            }

                            ++e1;
                        }
                    }
                    else {
                        while (*e1!='\\' && e1<e)
                            ++e1;
                    }

                    if (*s=='a') {
                        if (isdigit(s[1])) { // \a<>
                            if (0==Alignment) {
                                int al = (int)s[1] - (int)'0';
                                if (0<al && al<12) {
                                    uint8 const align_table[12] = {
                                        10, 1, 2, 3,
                                        10, 7, 8, 9,
                                        10, 4, 5, 6,
                                    };

                                    if (align_table[al]<10) {
                                        Alignment = align_table[al];
                                    }
                                }
                            }
                        }
                        else if (s[1]=='n') { // \an<>
                            if (0==Alignment) {
                                int al = (int)s[2] - (int)'0';
                                if (0<al && al<10) {
                                    Alignment = (uint8) al;
                                }
                            }
                        }
                        else if (s[1]=='l'&&s[2]=='p'&&s[3]=='h'&&s[4]=='a') { // \alpha defaults to \1a
                            int alpha = 0;
                            if (s[5]=='&') { // always with this '&' ?
                                if (s+7<e1) {
                                    alpha = hex(s[6], s[7]);
                                    if (0<=alpha && alpha<=255) {
                                        color2.a = color1.a = (uint8) (255 - alpha);
                                    }
                                }
                            }
                            else {
                                alpha = atol(s+5);
                                if (0<=alpha && alpha<=255) {
                                    color2.a = color1.a = (uint8) (255 - alpha);
                                }
                            }
                        }
                    }
                    else if (*s=='b') {
                        if (s[1]=='1')
                            desired.bold = 1;
                        else if (s[1]=='0')
                            desired.bold = 0;
                        else if (s[1]=='o' && s[2]=='r' && s[3]=='d') { // \bord<>
                            if (isdigit(s[4])) {
                                int const t = atol(s+4);
                                if (0<=t && t<=4) {
                                    bord = (uint8) t;
                                }
                            }
                        }
                        // still, there is \be for "blur edges"
                    }
                    else if (*s=='i') {
                        if (s[1]=='1')
                            desired.italic = 1;
                        else if (s[1]=='0')
                            desired.italic = 0;
                    }
                    else if (*s=='u') {
                        if (s[1]=='1')
                            desired.underline = 1;
                        else if (s[1]=='0')
                            desired.underline = 0;
                    }
                    else if (*s=='s') {
                        if (s[1]=='1')
                            desired.strikeout = 1;
                        else if (s[1]=='0')
                            desired.strikeout = 0;
                        else if (s[1]=='h' && s[2]=='a' && s[3]=='d') { // \shad
                            if (isdigit(s[4])) {
                                int const t = atol(s+4);
                                if (0<=t && t<=4) {
                                    shad = (uint8) t;
                                }
                            }
                        }
                    }
                    else if (*s=='f') {
                        if (s[1]=='n') { // font name
                            s+=2;
                            if (s<e1) {
                                int len = (int)(e1 - s);
                                if (len<FontParam::MAX_NAME_LENGTH) {
                                    memcpy(desired.name, s, len);
                                    desired.name[len] = '\0';
                                }
                            }
                        }
                        else if (s[1]=='s') { // font size
                            s += 2;
                            if (isdigit(*s)) {
                                fs = atol(s);

                                // clear \t fs
                                animSet.t_fs_clear();
                                tCmd &= ~tCmd_fs;
                                if (tCmd==tCmd_on)
                                    tCmd = 0;
                            }
                            else if (*s=='c') {
                                if (s[1]=='x') {
                                    if (isdigit(s[2])) {
                                        fscx = atol(s+2);

                                        // clear \t fscx
                                        animSet.t_fscx_clear();
                                        tCmd &= ~tCmd_fscx;
                                        if (tCmd==tCmd_on)
                                            tCmd = 0;
                                    }
                                }
                                else if (s[1]=='y') {
                                    if (isdigit(s[2])) {
                                        fscy = atol(s+2);

                                        // clear \t fscy
                                        animSet.t_fscy_clear();
                                        tCmd &= ~tCmd_fscy;
                                        if (tCmd==tCmd_on)
                                            tCmd = 0;
                                    }
                                }
                            }
                            else if (*s=='p') { // \fsp, spacing
                            }
                        }
                        else if (s[1]=='r') { // rotate degree
                            if (s[2]=='x') {
                                frx = (float) atof(s+3);
                            }
                            else if (s[2]=='y') {
                                fry = (float) atof(s+3);
                            }
                            else {
                                frz = (float) atof(s+2+(s[2]=='z' ? 1:0));

                                animSet.t_fr_clear();
                                tCmd &= ~tCmd_fr;
                                if (tCmd==tCmd_on)
                                    tCmd = 0;
                            }
                        }
                        else if (s[1]=='e') { // encoding
                            if (isdigit(s[2]))
                                fe = atol(s+2);
                        }
                        else if (s[1]=='a' && s[2]=='d') {
                            char const* e2 = e1 - 1;
                            s += 3;
                            while (s<e2 && *e2!=')') {
                                --e2;
                            }

                            if (*e2==')') {
                                if (s[0]=='(') { // \fad(t1,t2)
                                    if (isdigit(s[1])) {
                                        int const t2 = atol(++s);
                                        if (0<=t2 && ++s<e2) {
                                            while (s<e2 && *s!=',') ++s;
                                            if (*s==',' && isdigit(s[1])) {
                                                fade.t2 = dialogue.TimeStart + t2;
                                                fade.t3 = dialogue.TimeEnd - atol(s+1);
                                                if (fade) {
                                                    fade.a1 = fade.a3 = 0; fade.a2 = 255;
                                                }
                                                else {
                                                    fade.Reset(dialogue.TimeStart, dialogue.TimeEnd);
                                                }
                                            }
                                        }
                                    }
                                }
                                else if (s[0]=='e') { // \fade(a1,a2,a3,t1,t2,t3,t4)
                                    if (s[1]=='(') {
                                        int values[7];
                                        int num = 0;
                                        s += 2;
                                        while (s<e2 && isdigit(*s)) {
                                            char const* e3 = s + 1;
                                            while (e3<e2 && *e3!=',') ++e3;

                                            values[num++] = atol(s);
                                            if (num<7) {
                                                s = e3 + 1;
                                            }
                                            else {
                                                if (e3!=e2) {
                                                    num = -num; // failed
                                                }
                                                break;
                                            }
                                        }

                                        if (num==7 && 
                                            0<=values[0] && values[0]<=255 &&
                                            0<=values[1] && values[1]<=255 &&
                                            0<=values[2] && values[2]<=255 &&
                                            0<=values[3] && values[3]<=values[4] &&
                                            values[4]<=values[5] && values[5]<=values[6]) {

                                            fade.t1 = dialogue.TimeStart + values[3];
                                            fade.t2 = dialogue.TimeStart + values[4];
                                            fade.t3 = dialogue.TimeStart + values[5];
                                            fade.t4 = dialogue.TimeStart + values[6];
                                            fade.a1 = (uint8) (255 - values[0]);
                                            fade.a2 = (uint8) (255 - values[1]);
                                            fade.a3 = (uint8) (255 - values[2]);
                                            if (!fade) {
                                                fade.Reset(dialogue.TimeStart, dialogue.TimeEnd);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    else if (*s=='c' && s[1]=='&' && s[2]=='H') {
                        char const* clr = ++s; s+=2;
                        char const* e2 = e1 - 1;
                        while (s<e2 && *e2!='&') {
                            --e2;
                        }
                        if (*e2=='&') {
                            interpret_color(color1, clr, e2);
                        }
                    }
                    else if ('1'<=s[0] && s[0]<='4' && s[2]=='&' && s[3]=='H') {
                        int const c = s[0] - '0';
                        if (s[1]=='c') {
                            s += 2;
                            char const* e2 = e1 - 1;
                            while (s<e2 && *e2!='&') {
                                --e2;
                            }

                            if (s<e2 &&*e2=='&') {
                                if (1==c) {
                                    interpret_color(color1, s, e2);
                                }
                                else if (2==c) {
                                    interpret_color(color2, s, e2);
                                }
                                else if (3==c) {
                                    interpret_color(color3, s, e2);
                                }
                                else if (4==c) {
                                    interpret_color(color4, s, e2);
                                }
                            }
                        }
                        else if (s[1]=='a' && s[6]=='&') {
                            if (1==c) {
                                color1.a = 255 - hex(s[4], s[5]);
                            }
                            else if (2==c) {
                                color2.a = 255 - hex(s[4], s[5]);
                            }
                            else if (3==c) {
                                color3.a = 255 - hex(s[4], s[5]);
                            }
                            else if (4==c) {
                                color4.a = 255 - hex(s[4], s[5]);
                            }
                        }
                    }
                    else if (*s=='q') {
                        if ('0'<=s[1] && s[1]<='3') {
                            WrapMode = s[1] - '0';
                        }
                    }
                    else if (*s=='r') {
                        // cancels all previous style
                        int s_id2 = styleId;
                        if (++s<e1) {
                            ///////////////////////////////////////////////////////////////////////
                            // FIX-ME : note we had translate encoding to UTF-8
                            // if source is not UTF-8 at first place, this hash could be wrong!!!
                            ///////////////////////////////////////////////////////////////////////
                            uint32 const style_crc2 = mlabs::balai::CalcCRC((uint8 const*)s, (int)(e1-s)); 
                            if (style_crc2==Dialogue_Style_Default || style_crc2==Dialogue_Style_StarDefault) {
                                s_id2 = defaultStyle_;
                            }
                            else {
                                for (int i=0; i<totalStyles_; ++i) {
                                    if (style_crc2==styles_[i].Name) {
                                        s_id2 = i;
                                        break;
                                    }
                                }
                            }
                        }

                        // style reset
                        Style const& style2 = styles_[s_id2];
                        style2.BuildFontParam(desired);
                        color1 = style2.PrimaryColour;
                        color2 = style2.SecondaryColour;
                        color3 = style2.OutlineColour;
                        color4 = style2.BackColour;

                        fade.Reset(dialogue.TimeStart, dialogue.TimeEnd);
                        animSet.Reset(); tCmd = 0;

                        fs = style2.Fontsize;
                        fscx = style2.ScaleX;
                        fscy = style2.ScaleY;
                        fe = style2.Encoding;

                        frx = fry = 0.0f;
                        frz = style2.Angle;

                        bord_style = (3==style2.BorderStyle) ? 0x80:0x00;
                        bord = style2.Outline;
                        shad = style2.Shadow;

                        karaokeTime = 0;
                        karaoke = 0;
                        karaokeSequence = 0;

                        pMode = 0;
                    }
                    else if (*s=='K') {
                        if (isdigit(s[1])) { // \kf or \K<duration> fill up from left to right
                            karaokeTime = 10*atol(s+1); // hundredths of seconds
                            karaoke = 2;
                            ++karaokeSequence;
                        }
                    }
                    else if (*s=='k') { // Karaoke effect
                        if (isdigit(s[1])) {
                            // \k<duration> is the amount of time that
                            // each section of text is highlighted for
                            // in a dialogue event with Karaoke effect.
                            // the durations are in hundredths of seconds
                            // e.g. {\k94}This {\k48}is {\k24}a {\k150}karaoke {\k94}line
                            //
                            karaokeTime = 10*atol(s+1);
                            karaoke = 1;
                            ++karaokeSequence;
                        }
                        else if (s[1]=='f' && isdigit(s[2])) { // \kf or \K<duration> fill up from left to right
                            karaokeTime = 10*atol(s+2);
                            karaoke = 2;
                            ++karaokeSequence;
                        }
                        else if (s[1]=='o' && isdigit(s[2])) { // \ko<duration> outline highlighting from left to right
                            karaokeTime = 10*atol(s+2);
                            karaoke = 3;
                            ++karaokeSequence;
                        }
                    }
                    else if (*s=='p') {
                        if (isdigit(s[1])) {
                            pMode = ('0'!=s[1]);
                        }
                        else if (s[1]=='o' && s[2]=='s') {
                            if (s[3]=='(' && 0==moveCmd) { // pos(x,y) = move(x,y,x,y,0,0)
                                float const x = (float) atof(s+=4);
                                char const* e2 = e1 - 1;
                                while (s<e2 && *e2!=')') {
                                    --e2;
                                }
                                if (*e2==')') {
                                    ++s;
                                    while (s<e2 && *s!=',') {
                                        ++s;
                                    }
                                    if (*s==',') {
                                        moveX1 = moveX2 = x;
                                        moveY1 = moveY2 = (float) atof(s+1);
                                        moveT1 = dialogue.TimeStart;
                                        moveT2 = dialogue.TimeEnd;
                                        moveCmd = 1;
                                    }
                                }
                            }
                        }
                    }
                    else if (*s=='o'&&s[1]=='r'&&s[2]=='g') {
                        if (s[3]=='(' && 0==orgCmd) { // org(x,y)
                            float const x = (float) atof(s+=4);
                            char const* e2 = e1 - 1;
                            while (s<e2 && *e2!=')') {
                                --e2;
                            }
                            if (*e2==')') {
                                ++s;
                                while (s<e2 && *s!=',') {
                                    ++s;
                                }
                                if (*s==',') {
                                    orgX = x;
                                    orgY = (float) atof(s+1);
                                    orgCmd = 1;
                                }
                            }
                        }
                    }
                    else if (*s=='m'&&s[1]=='o'&&s[2]=='v'&&s[3]=='e') {
                        if (s[4]=='(' && 0==moveCmd) { // move(x1,y1,x2,y2[,t1,t2])
                            float move[4];
                            int t1(dialogue.TimeStart), t2(dialogue.TimeEnd);
                            int num = 0;
                            move[num++] = (float) atof(s+=5);

                            char const* e2 = e1 - 1;
                            while (s<e2 && *e2!=')') {
                                --e2;
                            }
                            if (*e2==')') {
                                ++s;
                                while (num<6 && s<e2) {
                                    if (*s==',') {
                                        if (num<4) {
                                            move[num++] = (float) atof(++s);
                                        }
                                        else if (4==num) {
                                            t1 = dialogue.TimeStart + atoi(++s);
                                            ++num;
                                        }
                                        else if (5==num) {
                                            t2 = dialogue.TimeStart + atoi(++s);
                                            ++num;
                                        }
                                    }
                                    ++s;
                                }

                                if ((4==num || 6==num) &&
                                    dialogue.TimeStart<=t1 && t1<t2 && t2<=dialogue.TimeEnd) {
                                    moveX1 = move[0];
                                    moveY1 = move[1];
                                    moveX2 = move[2];
                                    moveY2 = move[3];
                                    moveT1 = t1;
                                    moveT2 = t2;
                                    moveCmd = (4==num) ? 2:3;
                                }
                            }
                        }
                    }
                    else if (*s=='t') {
                        //
                        // TO-DO : animation function \t
                        // e.g. 
                        //   1) \t(500,2500,\fs15)
                        //   2) \t(0,600,\fs100\t(600,2500,\fs57))
                        //
                        char const* t[3];
                        int stack = 0;
                        while (s<e1 && s[0]=='t' && s[1]=='(') {
                            ++stack;
                            t[1] = t[2] = NULL;
                            char const* e2 = t[0] = s + 2;
                            int c = 1;
                            while (e2<e1 && *e2!='\\') {
                                if (*e2==',') {
                                    if (c<3) {
                                        t[c] = e2 + 1;
                                    }
                                    ++c;
                                }
                                ++e2;
                            }

                            int t1(dialogue.TimeStart), t2(dialogue.TimeEnd);
                            float accl = 1.0f;
                            if (*e2=='\\' && c<=4) {
                                tCmd |= tCmd_on;
                                if (c>2) {
                                    t1 = dialogue.TimeStart + atoi(t[0]);
                                    t2 = dialogue.TimeStart + atoi(t[1]);
                                    if (4==c) {
                                        accl = (float) atof(t[2]);
                                    }
                                }
                                else if (2==c) {
                                    accl = (float) atof(t[0]);
                                }

                                s = e2 + 1;
                                if (s<e1) {
                                    if (s[0]=='f') {
                                        if (s[1]=='s') {
                                            if (isdigit(s[2])) {
                                                // \fs
                                                animSet.t_fs(t1, t2, accl, atoi(s+2));
                                                tCmd |= tCmd_fs;
                                            }
                                            else if ('c'==s[2]) {
                                                if ('x'==s[3]) {
                                                    // \fscx
                                                    animSet.t_fscx(t1, t2, accl, atoi(s+4));
                                                    tCmd |= tCmd_fscx;
                                                }
                                                else if ('y'==s[3]) {
                                                    // \fscy
                                                    animSet.t_fscy(t1, t2, accl, atoi(s+4));
                                                    tCmd |= tCmd_fscy;
                                                }
                                            }
                                            else if ('p'==s[2]) {
                                                // \fsp
                                                //c = atoi(s+3);
                                            }
                                        }
                                        else if (s[1]=='r') {
                                            // \fr, rotation angle frz
                                            animSet.t_fr(t1, t2, accl, (int) (100.0f*atof(s+2)));
                                            tCmd |= tCmd_fr;
                                        }
                                    }
                                    else if (s[0]=='c') {
                                        if ('&'==s[1]) { // \c, not alpha
                                            if ('H'==s[2]) {
                                            }
                                        }
                                        else if (s[1]=='l' && s[2]=='i' && s[3]=='p') {
                                            // \clip
                                        }
                                    }
                                    else if (s[0]=='a' && s[1]=='l' && s[2]=='p' && s[3]=='h' && s[4]=='a') {
                                        // \alpha
                                    }
                                    else if ('1'<=s[0] && s[0]<='4') {
                                        if ('c'==s[1]) { // \1-4c, not alpha
                                        }
                                        else if ('a'==s[1]) { // \1-4a
                                        }
                                    }
                                }

                                while (s<e1) {
                                    if (*s=='\\') {
                                        ++s;
                                        break;
                                    }
                                    else if (*s==')') {
                                        --stack;
                                    }
                                    ++s;
                                }
                            }
                            else {
                                break;
                            }
                        }
                    }
                    s = e1;
                }
                s = e;

                // parameters changed..
                if (pg->length>0) {
                    if (karaoke || 
                        pg->font!=desired || pg->fs != fs ||
                        pg->color1 != color1 || /*pg->color2 != color2 ||*/
                        pg->color3 != color3 || pg->color4 != color4 ||

                        pg->fscx != fscx || pg->fscy != fscy ||

                        /* pg->fe != fe || */

                        !(pg->animSet.Equals(animSet)) ||
                        !(pg->fade.Equals(fade)) ||

                        pg->frz != frz || pg->frx != frx || pg->fry != fry ||

                        pg->bord != ((bord||shad) ? (bord_style|bord):0) || pg->shad != shad ||

                        pg->pMode != pMode) {

                        // start a new paragraph
                        if (++num_paragraphs>=MAX_NUM_SUBTITLE_RECTS) {
                            break;
                        }

                        pg = paragraphs + num_paragraphs;
                        pg->text = ptr; *ptr = '\0';
                        pg->length = 0;
                    }
                }
 
                pg->font = desired;
                pg->color1 = color1;
                pg->color2 = color2;
                pg->color3 = color3;
                pg->color4 = color4;
                pg->fs = fs;
                pg->fscx = fscx;
                pg->fscy = fscy;
                pg->fe = fe;

                pg->animSet = animSet;
                pg->fade = fade;

                pg->frx = frx;
                pg->fry = fry;
                pg->frz = frz;

                pg->bord = (bord||shad) ? (bord_style|bord):0;
                pg->shad = shad;
                pg->pMode = pMode;

                pg->karaokeTime = karaokeTime;
                pg->karaoke = karaoke;
                pg->karaokeSequence = karaokeSequence;
            }
            else if (s[0]=='\\'&&(s[1]=='n'||s[1]=='N')) {
                *ptr++ = '\n';
                ++(pg->length);
                ++s;
            }
            else {
                *ptr++ = *s;
                ++(pg->length);
            }
            ++s;
        }

        if (pg->length>0) {
            ++num_paragraphs;
        }
        else if (num_paragraphs<=0) {
            continue;
        }

        // collect paragraphes of this dialogue...
        DialogueRect& dial_rect = dialogueRects[num_dialogueRects];
        dial_rect.glyphes = textGlyphes + num_rects;
        dial_rect.rects = rects + num_rects;
        dial_rect.num_rects = 0;
        dial_rect.left = dial_rect.top = 0.0f;
        dial_rect.width = dial_rect.height = 0;

        dial_rect.time0 = dialogue.TimeStart;
        dial_rect.time_start = (dial_rect.time0<timestamp) ? timestamp:dial_rect.time0;
        dial_rect.time_end = dialogue.TimeEnd;

        // \pos or \move
        dial_rect.moveCmd = moveCmd;
        if (dial_rect.moveCmd) {
            dial_rect.moveX1 = moveX1*corrected_PlayResX/playResX_;
            dial_rect.moveY1 = moveY1*corrected_PlayResY/playResY_;
            dial_rect.moveX2 = moveX2*corrected_PlayResX/playResX_;
            dial_rect.moveY2 = moveY2*corrected_PlayResY/playResY_;
            dial_rect.moveT1 = moveT1;
            dial_rect.moveT2 = moveT2;
        }
        else {
            dial_rect.moveX1 = dial_rect.moveY1 = 0.0f;
            dial_rect.moveX2 = dial_rect.moveY2 = 0.0f;
            dial_rect.moveT1 = dial_rect.moveT2 = 0;
        }

        // \org
        dial_rect.orgCmd = orgCmd;
        if (dial_rect.orgCmd) {
            dial_rect.orgX = orgX*corrected_PlayResX/playResX_;
            dial_rect.orgY = orgY*corrected_PlayResY/playResY_;
        }
        else {
            dial_rect.orgX = dial_rect.orgY = 0.0f;
        }

        // \t
        dial_rect.tCmd = tCmd;

        // alignment
        dial_rect.alignment = (0<Alignment && Alignment<10) ? Alignment:style.Alignment;

        // miscs
        dial_rect.karaokeOn = 0;
        dial_rect.rotateOn = 0;

        // margin
        dial_rect.marginL = (dialogue.MarginL!=0) ? dialogue.MarginL:style.MarginL;
        dial_rect.marginR = (dialogue.MarginR!=0) ? dialogue.MarginR:style.MarginR;
        dial_rect.marginV = (dialogue.MarginV!=0) ? dialogue.MarginV:style.MarginV;

        // error check
        if ((100*dial_rect.marginL)>(5*playResX_)) {
            dial_rect.marginL = 5*playResX_/100;
        }
        if ((100*dial_rect.marginR)>(5*playResX_)) {
            dial_rect.marginR = 5*playResX_/100;
        }
        if ((100*dial_rect.marginV)>(3*playResY_)) {
            dial_rect.marginV = 3*playResY_/100;
        }

        // rescale
        dial_rect.marginL = dial_rect.marginL*corrected_PlayResX/playResX_;
        dial_rect.marginR = dial_rect.marginR*corrected_PlayResX/playResX_;
        dial_rect.marginV = dial_rect.marginV*corrected_PlayResY/playResY_;

        // max width to truncate length text
        int const max_subtitle_display_width = corrected_PlayResX - dial_rect.marginL - dial_rect.marginR;

        int display_left(0), display_top(0);
        int ww(0), hh(0), display_height_max(0);
        for (int i=0; i<num_paragraphs; ++i) {
            pg = paragraphs + i;
            if (pg->pMode) // ignore drawing mode
                continue;

            if (!IsFontAvailable(pg->font, ISO_639_UNKNOWN, charset)) {
                // if font is not installed, then Arial will be used instead
                memcpy(pg->font.name, "Arial", 6);
            }

            if (pg->font!=current) {
                SetFont_(pg->font, charset);
                current = pg->font;
            }

            int const display_height = (pg->fs*corrected_PlayResY/playResY_)*font_size_adj_num/font_size_adj_den;
            int const default_display_size = (style.Fontsize*corrected_PlayResY/playResY_)*font_size_adj_num/font_size_adj_den;

            int const dist_border_size = textBlitter_.GetDistanceMapSafeBorder(TEXT_FONT_PYRAMID);
            char const* s = pg->text;
            char const* const text_end = pg->text + pg->length;
            while (s<text_end && num_rects<max_rects) {
                if (*s=='\n') {
                    display_left = 0;
                    if (display_height_max>0) {
                        display_top += display_height_max;
                        display_height_max = 0;
                    }
                    else {
                        display_top += display_height;
                    }
                    ++s;
                    continue;
                }

                char const* e = s;
                while (e<text_end && *e!='\n') ++e;
                wchar_t const* const wptr_end = wptr + MultiByteToWideChar(CP_UTF8, 0, s, (int) (e-s), wptr, (int)(wtext_buffer_end-wptr));
                if (wptr<wptr_end) {
                    int length(0), linebreaks(0);
                    while (wptr<wptr_end && num_rects<max_rects &&
                           DialogueLineBreaker_(ww, hh, display_left, length, linebreaks,
                                                wptr, wptr_end,
                                                (0==(tCmd&tCmd_fs)) ? display_height:default_display_size,
                                                 max_subtitle_display_width)) {
                        if (0!=(linebreaks&2)) {
                            assert(0==display_left);
                            display_top += display_height_max;
                            display_height_max = 0;
                        }

                        TextGlyph& glyph = dial_rect.glyphes[dial_rect.num_rects];
                        glyph.font = pg->font;
                        glyph.text = wptr;
                        glyph.length = length;
                        glyph.display_top = display_top;
                        glyph.display_width = display_height*ww/hh;
                        glyph.display_height = display_height;

                        // texture size(over estimate)
                        glyph.texture_width_in = ww;
                        glyph.texture_height_in = hh;
                        glyph.texture_width_out = ww + 2*dist_border_size;
                        glyph.texture_height_out = hh + 2*dist_border_size;

                        // karaoke
                        glyph.karaokeTime = pg->karaokeTime;
                        glyph.karaokeSequence = pg->karaokeSequence;

                        if (width<glyph.texture_width_out)
                            width = glyph.texture_width_out;

                        if (display_height_max<display_height)
                            display_height_max = display_height;

                        display_left += glyph.display_width;
                        if (dial_rect.width<display_left) {
                            dial_rect.width = display_left;
                        }

                        SubtitleRect& rect = dial_rect.rects[dial_rect.num_rects];
                        rect.Reset();

                        if (pg->fade) {
                            SubtitleRect::sAnimationKey const anm[4] = {
                                SubtitleRect::sAnimationKey(pg->fade.t1, pg->fade.a1),
                                SubtitleRect::sAnimationKey(pg->fade.t2, pg->fade.a2),
                                SubtitleRect::sAnimationKey(pg->fade.t3, pg->fade.a2),
                                SubtitleRect::sAnimationKey(pg->fade.t4, pg->fade.a3),
                            };
                            rect.SetFadeAnimation(anm, 4);
                        }

                        if (pg->fscx!=100) {
                            rect.ScaleX = 0.01f * pg->fscx;
                        }

                        if (pg->fscy!=100) {
                            rect.ScaleY = 0.01f * pg->fscy;
                        }

                        if (pg->animSet) {
                            AnimationSet const& as = pg->animSet;
                            SubtitleRect::sAnimationKey animKeys[SubtitleRect::ANIMATION_BUFFER_SIZE];
                            int total_keys, t1, v1;
                            if (as.fs) {
                                t1 = -1;
                                v1 = pg->fs;
                                total_keys = 0;
                                for (int i=0; i<as.fs.nKeys && total_keys<SubtitleRect::ANIMATION_BUFFER_SIZE; ++i) {
                                    int const id = as.fs.KeyIds[i];
                                    assert(id<as.nKeys);
                                    if (id<as.nKeys) {
                                        AnimationSet::AnimationKey const& k = as.Keys[id];
                                        if (t1!=k.t1) {
                                            SubtitleRect::sAnimationKey& key = animKeys[total_keys++];
                                            key.Time = k.t1;
                                            key.Value = 2000*v1/pg->fs;
                                            key.Accl = 1.0f; // what ever
                                        }

                                        if (total_keys<SubtitleRect::ANIMATION_BUFFER_SIZE) {
                                            SubtitleRect::sAnimationKey& key = animKeys[total_keys++];
                                            key.Time = t1 = k.t2;
                                            key.Value = 2000*(v1=k.value)/pg->fs;
                                            key.Accl = k.accl;
                                        }
                                    }
                                }

                                rect.SetSizeAnimation(animKeys, total_keys);
                            }

                            if (animSet.fr) {
                                t1 = -1;
                                v1 = (int) (100.0f*pg->frz);
                                total_keys = 0;
                                for (int i=0; i<as.fr.nKeys && total_keys<SubtitleRect::ANIMATION_BUFFER_SIZE; ++i) {
                                    int const id = as.fr.KeyIds[i];
                                    assert(id<as.nKeys);
                                    if (id<as.nKeys) {
                                        AnimationSet::AnimationKey const& k = as.Keys[id];
                                        if (t1!=k.t1) {
                                            SubtitleRect::sAnimationKey& key = animKeys[total_keys++];
                                            key.Time = k.t1;
                                            key.Value = v1;
                                            key.Accl = 1.0f; // whatever
                                        }

                                        if (total_keys<SubtitleRect::ANIMATION_BUFFER_SIZE) {
                                            SubtitleRect::sAnimationKey& key = animKeys[total_keys++];
                                            key.Time = t1 = k.t2;
                                            key.Value = v1 = k.value;
                                            key.Accl = k.accl;
                                        }
                                    }
                                }

                                rect.SetRollAnimation(animKeys, total_keys);
                            }
/*
                            if (animSet.fscx) {
                                t1 = -1;
                                v1 = pg->fscx;
                                total_keys = 0;
                                for (int i=0; i<as.fscx.nKeys && total_keys<SubtitleRect::ANIMATION_BUFFER_SIZE; ++i) {
                                    int const id = as.fscx.KeyIds[i];
                                    assert(id<as.nKeys);
                                    if (id<as.nKeys) {
                                        AnimationSet::AnimationKey const& k = as.Keys[id];
                                        if (t1!=k.t1) {
                                            SubtitleRect::sAnimationKey& key = animKeys[total_keys++];
                                            key.Time = k.t1;
                                            key.Value = 100*v1/pg->fscx;
                                            key.Accl = 1.0f; // what ever
                                        }

                                        if (total_keys<SubtitleRect::ANIMATION_BUFFER_SIZE) {
                                            SubtitleRect::sAnimationKey& key = animKeys[total_keys++];
                                            key.Time = t1 = k.t2;
                                            key.Value = 100*(v1=k.value)/pg->fscx;
                                            key.Accl = k.accl;
                                        }
                                    }
                                }

                                rect.SetScaleXAnimation(animKeys, total_keys);
                            }

                            if (animSet.fscy) {
                                t1 = -1;
                                v1 = pg->fscy;
                                total_keys = 0;
                                for (int i=0; i<as.fscy.nKeys && total_keys<SubtitleRect::ANIMATION_BUFFER_SIZE; ++i) {
                                    int const id = as.fscy.KeyIds[i];
                                    assert(id<as.nKeys);
                                    if (id<as.nKeys) {
                                        AnimationSet::AnimationKey const& k = as.Keys[id];
                                        if (t1!=k.t1) {
                                            SubtitleRect::sAnimationKey& key = animKeys[total_keys++];
                                            key.Time = k.t1;
                                            key.Value = 100*v1/pg->fscy;
                                            key.Accl = 1.0f; // what ever
                                        }

                                        if (total_keys<SubtitleRect::ANIMATION_BUFFER_SIZE) {
                                            SubtitleRect::sAnimationKey& key = animKeys[total_keys++];
                                            key.Time = t1 = k.t2;
                                            key.Value = 100*(v1=k.value)/pg->fscy;
                                            key.Accl = k.accl;
                                        }
                                    }
                                }

                                rect.SetScaleYAnimation(animKeys, total_keys);
                            }
*/
                        }

                        rect.PrimaryColour = pg->color1;
                        rect.SecondaryColour = pg->color2;
                        rect.OutlineColour = pg->color3;
                        rect.BackColour = pg->color4;

                        rect.TimeStart = dial_rect.time_start;
                        rect.TimeEnd  = dial_rect.time_end;

                        rect.Layer = dialogue.Layer; // level

                        rect.PlayResX = (float) corrected_PlayResX;
                        rect.PlayResY = (float) corrected_PlayResY;

                        rect.Shadow = pg->shad;
                        rect.Border = pg->bord;

                        // #1...#9 numpad
                        assert(0<dial_rect.alignment && dial_rect.alignment<10);
                        rect.Alignment = uint8(dial_rect.alignment);

                        // f
                        if ((2!=dial_rect.alignment) ||
                            dial_rect.tCmd || dial_rect.moveCmd || dial_rect.orgCmd) {
                            rect.Flags |= uint8(1<<7); // fix pos
                            if (dial_rect.tCmd) rect.Flags |= uint8(1<<6);
                            if (dial_rect.moveCmd>1) rect.Flags |= uint8(1<<5);
                            if (dial_rect.orgCmd) rect.Flags |= uint8(1<<4);
                        }

                        rect.Karaoke = pg->karaoke;
                        if (rect.Karaoke) {
                            dial_rect.karaokeOn = 1;
                        }

                        if (0.0f!=pg->frx || 0.0f!=pg->frx || 0.0f!=pg->frz ||
                            0!=(dial_rect.tCmd & tCmd_fr)) {
                            // right-handed coordinate system with Z up.
                            // also convert from degree to radian
                            rect.Yaw = pg->fry*0.017453f; // 
                            rect.Roll = -pg->frz*0.017453f;
                            rect.Pitch = -pg->frx*0.017453f;
                            dial_rect.rotateOn = 1;
                        }

                        ++dial_rect.num_rects;
                        ++num_rects;

                        if ((wptr+=length)<wptr_end || 0!=(linebreaks&1)) {
                            display_left = 0;
                            display_top += display_height_max;
                            display_height_max = 0;
                            while (wptr<wptr_end && (L' '==*wptr||0x3000==*wptr)) {
                                ++wptr;
                            }
                        }
                    }
                }

                // must respect this linebreak set by ASS \N, or \n
                if (e<text_end && *e=='\n') {
                    display_left = 0;
                    display_top += display_height_max;
                    display_height_max = 0;
                }

                s = e + 1;
            }
        }

        if (dial_rect.num_rects>0) {
            ++num_dialogueRects;
            dial_rect.height = display_top + display_height_max;

            int const alignX = dial_rect.alignment%3;
            int const alignY = (dial_rect.alignment-1)/3;
            if (dial_rect.moveCmd) {
                if (2==alignX) { // 2, 5, 8 : center aligned
                    dial_rect.left = dial_rect.moveX1 - dial_rect.width/2;
                }
                else if (0==alignX) { // 3, 6, 9 : right aligned
                    dial_rect.left = dial_rect.moveX1 - dial_rect.width;
                }
                else { // 1, 4, 7 : left aligned
                    dial_rect.left = dial_rect.moveX1;
                }

                if (0==alignY) { // 1, 2, 3 : bottom
                    dial_rect.top = dial_rect.moveY1 - dial_rect.height;
                }
                else if (1==alignY) { // 4, 5, 6 : center
                    dial_rect.top = dial_rect.moveY1 - dial_rect.height/2;
                }
                else { // 7, 8, 9 : top
                    dial_rect.top = dial_rect.moveY1;
                }
            }
            else {
                if (2==alignX) { // 2, 5, 8 : center aligned
                    dial_rect.left = 0.5f*(corrected_PlayResX - dial_rect.width);
                }
                else if (0==alignX) { // 3, 6, 9 : right aligned
                    dial_rect.left = (float) (corrected_PlayResX - dial_rect.marginR - dial_rect.width);
                }
                else { // 1, 4, 7 : left aligned
                    dial_rect.left = (float) dial_rect.marginL;
                }

                if (0==alignY) { // 1, 2, 3 : bottom
                    dial_rect.top = (float) (corrected_PlayResY - dial_rect.marginV - dial_rect.height);
                }
                else if (1==alignY) { // 4, 5, 6 : center
                    dial_rect.top = 0.5f*(corrected_PlayResY - dial_rect.height);
                }
                else { // 7, 8, 9 : top
                    dial_rect.top = (float) dial_rect.marginV;
                }
            }
        }
    } // for each dialogue

    if (num_rects<0)
        return 0;

    // conservative determine width.
    width = (width + TEXT_FONT_SPACE_GUARDBAND + 3) & ~3;
    if (width<512) width = 512;

    // and height
    int hh(0), s0(0), t0(0);
    for (int i=0; i<num_rects; ++i) {
        TextGlyph& glyph = textGlyphes[i];
        if ((s0+=(glyph.texture_width_out+TEXT_FONT_SPACE_GUARDBAND))>width) {
            t0 += (hh+TEXT_FONT_SPACE_GUARDBAND);
            s0 = glyph.texture_width_out + TEXT_FONT_SPACE_GUARDBAND;
            hh = glyph.texture_height_out;
        }
        else if (hh<glyph.texture_height_out) {
            hh = glyph.texture_height_out;
        }
    }
    height = t0 + hh;// + TEXT_FONT_SPACE_GUARDBAND;

    int const required_size = width*height;
    if (required_size>buffer_size) {
        return -required_size;
    }

    // clear image
    memset(buffer, 0, required_size);

    s0 = t0 = hh = 0;
    for (int i=0; i<num_rects; ++i) {
        TextGlyph& glyph = textGlyphes[i];
        if ((s0+glyph.texture_width_out+TEXT_FONT_SPACE_GUARDBAND)>width) {
            t0 += (hh+TEXT_FONT_SPACE_GUARDBAND);
            s0 = hh = 0;
        }

        if (glyph.font!=current) {
            SetFont_(glyph.font, charset);
            current = glyph.font;
        }

        textBlitter_.DistanceMap(buffer + width*t0 + s0,
                                 glyph.texture_width_out, glyph.texture_height_out,
                                 glyph.texture_width_in, glyph.texture_height_in,
                                 width, TEXT_FONT_PYRAMID, glyph.text, glyph.length);

        SubtitleRect& rect = rects[i];
        rect.Width = (float) glyph.display_width*glyph.texture_width_out/glyph.texture_width_in;
        rect.Height = (float) glyph.display_height*glyph.texture_height_out/glyph.texture_height_in;
        rect.s0 = (float)s0/(float)width;
        rect.s1 = (float)(s0+glyph.texture_width_out)/(float)width;
        rect.t0 = (float)t0/(float)height;
        rect.t1 = (float)(t0+glyph.texture_height_out)/(float)height;

        s0 += (glyph.texture_width_out+TEXT_FONT_SPACE_GUARDBAND);
        if (hh<glyph.texture_height_out)
            hh = glyph.texture_height_out;
    }
    assert((t0+hh)<=height);

    if (current!=font_default) {
        SetFont_(font_default, charset);
    }

    //
    // resolve collision
    if (COLLISION_NORMAL==collisions_) {
        for (int i=1; i<num_dialogueRects; ++i) {
            DialogueRect& d = dialogueRects[i];
            if (0==d.tCmd && 0==d.moveCmd && 0==d.orgCmd) {
                float const d_right = d.left + d.width;
                float d_bottom = d.top + d.height;
                int resolve = 0;
                while (0==resolve) {
                    resolve = 1;
                    for (int j=0; j<i; ++j) {
                        DialogueRect& d0 = dialogueRects[j];
                        if (0==d0.tCmd && 0==d0.moveCmd && 0==d0.orgCmd &&
                            d.time_start<d0.time_end && d.time_end>d0.time_start &&
                            d.left<(d0.left+d0.width) && d_right>d0.left &&
                            d.top<(d0.top+d0.height) && d_bottom>d0.top) {
                            resolve = 0;
                            if (d.alignment<7) {
                                d.top = d0.top - d.height - 1.0f; // move up
                                d_bottom = d.top + d.height;
                                if (d_bottom<0.0f)
                                    resolve = -1;
                            }
                            else {
                                d.top = d0.top + d0.height + 1.0f; // move down
                                d_bottom = d.top + d.height;
                                if (d.top>=corrected_PlayResY)
                                    resolve = -1;
                            }
                            break;
                        }
                    }
                }
            }
        }
    }
    else { // reverse
        for (int i=num_dialogueRects-2; i>=0; --i) {
            DialogueRect& d = dialogueRects[i];
            if (0==d.tCmd && 0==d.moveCmd && 0==d.orgCmd) {
                float const d_right = d.left + d.width;
                float d_bottom = d.top + d.height;
                int resolve = 0;
                while (0==resolve) {
                    resolve = 1;
                    for (int j=i+1; j<num_dialogueRects; ++j) {
                        DialogueRect& d0 = dialogueRects[j];
                        if (0==d0.tCmd && 0==d0.moveCmd && 0==d0.orgCmd &&
                            d.time_start<d0.time_end && d.time_end>d0.time_start &&
                            d.left<(d0.left+d0.width) && d_right>d0.left &&
                            d.top<(d0.top+d0.height) && d_bottom>d0.top) {
                            resolve = 0;
                            if (d.alignment<7) {
                                d.top = d0.top - d.height - 1.0f; // move up
                                d_bottom = d.top + d.height;
                                if (d_bottom<0.0f)
                                    resolve = -1;
                            }
                            else {
                                d.top = d0.top + d0.height + 1.0f; // move down
                                d_bottom = d.top + d.height;
                                if (d.top>=corrected_PlayResY)
                                    resolve = -1;
                            }
                            break;
                        }
                    }
                }
            }
        }
    }

    //
    // final reposition
    for (int i=0; i<num_dialogueRects; ++i) {
        DialogueRect& d = dialogueRects[i];
        int const alignX = d.alignment%3;
        int const alignY = (d.alignment-1)/3;

        float Xref = d.left; // 1, 4, 7 : left aligned
        if (2==alignX) { // 2, 5, 8 : center aligned
            Xref = d.left + 0.5f*d.width;
        }
        else if (0==alignX) { // 3, 6, 9 : right aligned
            Xref = d.left + d.width;
        }

        float Yref = d.top; // 7, 8, 9 : top
        if (0==alignY) { // 1, 2, 3 : bottom
            Yref = d.top + d.height;
        }
        else if (1==alignY) { // 4, 5, 6 : center
            Yref = d.top + 0.5f*d.height;
        }

        //
        // solve rect.x, rect.y
        uint8 lines = 0;
        for (int j=0; j<d.num_rects; ) {
            int k = j;
            int ww = hh = 0;
            int const display_top = d.glyphes[j].display_top;
            for (; j<d.num_rects && display_top==d.glyphes[j].display_top; ++j) {
                ww += d.glyphes[j].display_width;
                if (hh<d.glyphes[j].display_height)
                    hh = d.glyphes[j].display_height;
            }
            float const bottom = d.top + display_top + hh;

            float left = d.left; // 1, 4, 7 : left aligned
            if (2==alignX) { // 2, 5, 8 : center aligned
                left += 0.5f*(d.width - ww);
            }
            else if (0==alignX) { // 3, 6, 9 : right aligned
                left += d.width - ww;
            }

            for (; k<j; ++k) {
                SubtitleRect& rect = d.rects[k];
                TextGlyph const& glyph = d.glyphes[k];
                rect.X = left - 0.5f*(rect.Width-glyph.display_width);
                rect.Y = bottom - 0.5f*(rect.Height+glyph.display_height) +
                              0.25f*(glyph.display_height-hh); // baseline align(roughly)

                ///////////////////////////////////////////////////
                // it will 
                rect.Xpin = rect.X; // 1, 4, 7 : left aligned
                if (2==alignX) { // 2, 5, 8 : center aligned
                    rect.Xpin = rect.X + 0.5f*rect.Width;
                }
                else if (0==alignX) { // 3, 6, 9 : right aligned
                    rect.Xpin = rect.X + rect.Width;
                }

                rect.Ypin = rect.Y; // 7, 8, 9 : top
                if (0==alignY) { // 1, 2, 3 : bottom
                    rect.Ypin = rect.Y + rect.Height;
                }
                else if (1==alignY) { // 4, 5, 6 : center
                    rect.Ypin = rect.Y + 0.5f*rect.Height;
                }
                ///////////////////////////////////////////////////

                rect.Dialogue = (uint8) i;
                rect.Paragraph = (uint8) k;
                rect.Line = lines;
                if (d.moveCmd>1) {
                    SubtitleRect::sAnimationKey move[2];
                    if (d.moveX2!=d.moveX1) {
                        move[0].Time = d.moveT1;
                        move[0].Value = 0;
                        move[0].Accl = 1.0f;

                        move[1].Time = d.moveT2;
                        move[1].Value = (int) (100.0f*(d.moveX2 - d.moveX1));
                        move[1].Accl = 1.0f;
                        rect.SetMoveXAnimation(move, 2);
                    }
                    if (d.moveY2!=d.moveY1) {
                        move[0].Time = d.moveT1;
                        move[0].Value = 0;
                        move[0].Accl = 1.0f;

                        move[1].Time = d.moveT2;
                        move[1].Value = (int) (100.0f*(d.moveY2 - d.moveY1));
                        move[1].Accl = 1.0f;
                        rect.SetMoveYAnimation(move, 2);
                    }
                }

                left += glyph.display_width;
            }

            ++lines;
        }

        //
        // set rotate origin
        if (d.rotateOn) {
            if (0!=d.orgCmd) {
                Xref = d.orgX;
                Yref = d.orgY;
            }

            for (int j=0; j<d.num_rects; ++j) {
                SubtitleRect& rect = d.rects[j];
                rect.Xo = Xref; rect.Yo = Yref;
            }
        }

        //
        // set karaoke time
        if (d.karaokeOn) {
            int karaokeTime = d.time0;
            for (int j=0; j<d.num_rects; ) {
                if (d.rects[j].Karaoke) {
                    int const karaokeIndex = d.glyphes[j].karaokeSequence;
                    t0 = d.glyphes[j].karaokeTime;
                    s0 = 0;
                    int k = j;
                    for (; j<d.num_rects && karaokeIndex==d.glyphes[j].karaokeSequence; ++j) {
                        assert(t0==d.glyphes[j].karaokeTime);
                        s0 += d.glyphes[j].texture_width_out;
                    }

                    for (; k<j; ++k) {
                        SubtitleRect& rect = d.rects[k];
                        rect.TimeKaraokeIn = karaokeTime;
                        rect.TimeKaraokeOut = karaokeTime += t0*d.glyphes[k].texture_width_out/s0;
                    }
                }
                else {
                    ++j;
                }
            }
        }
    }
/*
    //
    // TO-DO : discard rects out of play area. (but never discard animated subtitles)
    int const num_rects_bak = num_rects;
    num_rects = 0;
    for (int i=0; i<num_rects_bak; ++i) {
        if (!rects[i].OutofBound()) {
            if (num_rects<i) {
                memcpy(rects+num_rects, rects+i, sizeof(SubtitleRect));
            }
            ++num_rects;
        }
    }
*/
    return num_rects;
}
//---------------------------------------------------------------------------------------
int Subtitle::Create(char const* videofile)
{
    Destroy();

    //
    // note videofile = "myvideo.mp4", "myvideo.mkv", "myvideo.wmv", "myvideo.mov"...
    // using utf8 encoding.
    //

    char const* ext = (NULL!=videofile) ? strrchr(videofile, '.'):NULL;
    if (NULL==ext || ext<=videofile)
        return 0;

    wchar_t wfilename[MAX_PATH];
    int len = (int) (ext - videofile);

    len = MultiByteToWideChar(CP_UTF8, 0, videofile, len, wfilename, MAX_PATH);
    if (len<=0 || (len+4)>=MAX_PATH)
        return 0;

    int const src_video_length = len;
    wfilename[len++] = L'*';
    wfilename[len++] = L'.';
    wfilename[len++] = L'*';
    wfilename[len++] = L'\0';

    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(wfilename, &fd);
    if (INVALID_HANDLE_VALUE!=hFind) {
        wchar_t wfullpath[MAX_PATH];
        memcpy(wfullpath, wfilename, len*sizeof(wchar_t));
        for (int i=len-3; i>=0; --i) {
            if (wfullpath[i]=='/') {
                wfullpath[i]='\0';
                break;
            }
        }

        do {
            if (0==(FILE_ATTRIBUTE_DIRECTORY&fd.dwFileAttributes)) {
                int short_len = (int) wcslen(fd.cFileName);
                if (short_len>4) {
                    wchar_t const* ext = fd.cFileName + short_len - 4;
                    if (*ext==L'.') {
                        ++ext;
                        int const loading_subtitle = totalExtSubtitleStreams_;
                        SubStream& sub = extSubtitleStreams_[loading_subtitle];
                        if (0==memcmp(ext, L"srt", 3*sizeof(wchar_t)) || 0==memcmp(ext, L"SRT", 3*sizeof(wchar_t))) {
                            len = swprintf(wfilename, MAX_PATH, L"%s/%s", wfullpath, fd.cFileName);
                            AddSubtitleStream_(wfilename, FORMAT_SRT);
                        }
                        else if (0==memcmp(ext, L"ass", 3*sizeof(wchar_t)) || 0==memcmp(ext, L"ASS", 3*sizeof(wchar_t))) {
                            len = swprintf(wfilename, MAX_PATH, L"%s/%s", wfullpath, fd.cFileName);
                            AddSubtitleStream_(wfilename, FORMAT_ASS);
                        }

                        if (loading_subtitle<totalExtSubtitleStreams_) {
                            int len2 = len - src_video_length;
                            if (len2>sizeof(sub.Filename)/sizeof(sub.Filename[0]))
                                len2 = sizeof(sub.Filename)/sizeof(sub.Filename[0]);
                            memcpy(sub.Filename, wfilename+len-len2, len2*sizeof(wchar_t));
                        }
                    }
                }
            }
        } while (0!=FindNextFileW(hFind, &fd) && totalExtSubtitleStreams_<MAX_EXTERNAL_SUBTITLE_STREAMS);

        FindClose(hFind);
    }

    return totalExtSubtitleStreams_;
}
//---------------------------------------------------------------------------------------
void Subtitle::Destroy()
{
    for (int i=0; i<totalExtSubtitleStreams_; ++i) {
        extSubtitleStreams_[i].Reset();
    }
    totalExtSubtitleStreams_    = 0;
    currentExtSubtitleStreamId_ = -1;
}
//---------------------------------------------------------------------------------------
bool Subtitle::Initialize()
{
    std::lock_guard<std::mutex> lock(mutex_);

    // set default style
    totalStyles_ = 1;
    defaultStyle_ = 0;
    FontParam font_settings;
    styles_[0].Reset();
    styles_[0].BuildFontParam(font_settings);
    if (SetFont_(font_settings, ISO_639_UNKNOWN)) {
        return true;
    }

    return false;
}
//---------------------------------------------------------------------------------------
void Subtitle::Finalize()
{
    std::lock_guard<std::mutex> lock(mutex_);
    Destroy();
    textBlitter_.Finalize();
}
//---------------------------------------------------------------------------------------
int Subtitle::ASS_Styles_(char const* begin, char const* end, ISO_639 lan, int encoding)
{
    if (NULL==begin || begin>=end)
        return -1;

    begin = newline(begin, end);
    if (0!=memcmp(begin, "[Script Info]", 13))
        return -1;
 
    begin += 13;
    struct Field {
        char const* name;
        uint32      hash;
        Field():name(NULL),hash(0) {};
    } styles[MAX_ASS_STYLE_FIELDS];
    int PlayResX(0), PlayResY(0), v4style(0), fields(0);
    char const* events = NULL;
    FontParam font_param;
    int default_style = totalStyles_ = 0;
    while (NULL==events && begin<end) {
        char const* e = begin + 1;
        while (*e!='\r' && *e!='\n' && e<end) {
            ++e;
        }

        char const* s = begin;
        if (e[0]=='\r' && e[1]=='\n') {
            begin = e + 2;
        }
        else {
            begin = e + 1;
        }

        if (*s=='[') {
            if (0==v4style) {
                // [V4+ Styles], [v4+ Styles], [V4+ styles], [v4+ styles],
                // [V4 Styles], [v4 Styles], [V4 styles], [v4 styles]
                if (('v'==s[1] || 'V'==s[1]) && '4'==s[2]) {
                    bool const v4plus = '+'==s[3];
                    s += v4plus ? 4:3;

                    if (' '==*s++ && ('s'==*s||'S'==*s) && 0==memcmp(s+1, "tyles]", 6)) {
                        v4style = v4plus ? 2:1;
                    }
                }
            }
            else if (0==memcmp(s+1, "Events]", 7)) {
                events = s;
            }
        }
        else if (v4style>0) {
            if (0<fields && 0==memcmp(s, "Style:", 6) && totalStyles_<MAX_SUBTITLE_STYLES) {
                s += 6;
                Style& style = styles_[totalStyles_];
                style.Reset();

                int f = 0;
                for (; f<fields && s<e; ++f) {
                    while (' '==*s && s<e) ++s;
                    char const* e1 = s + 1;
                    while (*e1!=',' && e1<e) ++e1;

                    Field const& ss = styles[f];
                    if (s<e1) {
                        int const len = (int)(e1-s);
                        if (ss.hash==Style_Name) {
                            if (0==memcmp(s, "Default", 7)) {
                                style.Name = 0;
                                default_style = totalStyles_;
                            }
                            else {
                                style.Name = mlabs::balai::CalcCRC((uint8 const*)s, len);
                            }
                        }
                        else if (ss.hash==Style_Fontname) {
                            if ((len+1)<sizeof(style.Fontname)) {
                                if (CP_UTF8!=encoding) {
                                    wchar_t wfontname[FontParam::MAX_NAME_LENGTH];
                                    int len2 = MultiByteToWideChar(encoding, 0, s, len, wfontname, FontParam::MAX_NAME_LENGTH);
                                    if (len2>0) {
                                        len2 = WideCharToMultiByte(CP_UTF8, 0, wfontname, len2, style.Fontname, FontParam::MAX_NAME_LENGTH, NULL, NULL);
                                        if (len2<FontParam::MAX_NAME_LENGTH)
                                            style.Fontname[len2] = '\0';
                                        else
                                            len2 = 0;
                                    }

                                    if (len2<3) {
                                        memcpy(style.Fontname, "Arial", 6);
                                    }
                                }
                                else {
                                    memcpy(style.Fontname, s, len);
                                    style.Fontname[len] = '\0';
                                }
                            }
                            else {
                                memcpy(style.Fontname, "Arial", 6);
                            }
                        }
                        else if (ss.hash==Style_Fontsize) {
                            style.Fontsize = atol(s);
                        }
                        else if (ss.hash==Style_PrimaryColour) {
                            interpret_color(style.PrimaryColour, s, e1);
                        }
                        else if (ss.hash==Style_SecondaryColour) {
                            interpret_color(style.SecondaryColour, s, e1);
                        }
                        else if (ss.hash==Style_OutlineColour) {
                            interpret_color(style.OutlineColour, s, e1);
                        }
                        else if (ss.hash==Style_BackColour) {
                            interpret_color(style.BackColour, s, e1);
                        }
                        else if (ss.hash==Style_Bold) {
                            style.Bold = (-1==atol(s)); // -1 is true, 0 is false
                        }
                        else if (ss.hash==Style_Italic) {
                            style.Italic = (-1==atol(s)); // -1 is true, 0 is false
                        }
                        else if (ss.hash==Style_Underline) {
                            style.Underline = (-1==atol(s));
                        }
                        else if (ss.hash==Style_Strikeout||ss.hash==Style_StrikeOut) {
                            style.StrikeOut = (-1==atol(s));
                        }
                        else if (ss.hash==Style_ScaleX) {
                            style.ScaleX = atol(s); // valid?
                        }
                        else if (ss.hash==Style_ScaleY) {
                            style.ScaleY = atol(s); // valid?
                        }
                        else if (ss.hash==Style_Spacing) {
                            style.Spacing = (uint8) atol(s); // valid?
                        }
                        else if (ss.hash==Style_Angle) {
                            style.Angle = (float) atof(s); // valid?
                        }
                        else if (ss.hash==Style_BorderStyle) {
                            style.BorderStyle = (3==atol(s)) ? 3:1;
                        }
                        else if (ss.hash==Style_Outline) {
                            style.Outline = (uint8) atol(s);
                        }
                        else if (ss.hash==Style_Shadow) {
                            style.Shadow = (uint8) atol(s);
                        }
                        else if (ss.hash==Style_Alignment) {
                            int alignemnt = atol(s);
                            if (0<alignemnt && alignemnt<=11) {
                                if (v4style<2) {
                                    uint8 const align_table[12] = {
                                        10, 1, 2, 3,
                                        10, 7, 8, 9,
                                        10, 4, 5, 6,
                                    };

                                    alignemnt = align_table[alignemnt];
                                }

                                if (alignemnt<=9) {
                                    style.Alignment = (uint8) alignemnt;
                                }
                                else {
                                    style.Alignment = 2;
                                }
                            }
                            else {
                                style.Alignment = 2;
                            }
                        }
                        else if (ss.hash==Style_MarginL) {
                            style.MarginL = atol(s);
                        }
                        else if (ss.hash==Style_MarginR) {
                            style.MarginR = atol(s);
                        }
                        else if (ss.hash==Style_MarginV) {
                            style.MarginV = atol(s);
                        }
                        else if (ss.hash==Style_AlphaLevel) {
                            if (isdigit(*s)) {
                                style.AlphaLevel = (uint8) atol(s);
                            }
                        }
                        else if (ss.hash==Style_Encoding) {
                            style.Encoding = atol(s);
                        }
                    }

                    s = e1 + 1;
                }

                if (f==fields) {
                    if (!style.CheckFontFace(lan, encoding)) {
                        memcpy(style.Fontname, "Arial", 6);
                    }

                    // style.FontSize OK!?
                    assert(style.Fontsize>0);
                    if (style.Fontsize<=0) {
                        style.Fontsize = playResY_*TEXT_DEFAULT_DISPLAY_HEIGHT_RATIO_PER_MILLI/1000;
                    }
                    ++totalStyles_;
                }
            }
            else if (0==memcmp(s, "Format:", 7)) {
                s += 7;
                while (fields<MAX_ASS_STYLE_FIELDS && s<e) {
                    while (' '==*s && s<e) ++s;
                    char const* e1 = s + 1;
                    while (*e1!=',' && e1<e) ++e1;

                    if (s<e1) {
                        Field& field = styles[fields++];
                        field.name = s;
                        field.hash = mlabs::balai::CalcCRC((uint8*)s, int(e1-s));
                    }

                    s = e1 + 1;
                }
            }
        }
        else { // still in [Script Info] section
            if (0==memcmp(s, "PlayResX:", 9)) {
                s = eatspace(s+9, e);
                if (s<e) {
                    PlayResX = atol(s);
                }
            }
            else if (0==memcmp(s, "PlayResY:", 9)) {
                s = eatspace(s+9, e);
                if (s<e) {
                    PlayResY = atol(s);
                }
            }
            else if (0==memcmp(s, "WrapStyle:", 10)) {
                s = eatspace(s+10, e);
                if (s<e && '0'<=*s && *s<='3') {
                    wrapMode_ = *s - '0';
                }
            }
            else if (0==memcmp(s, "Collisions:", 11)) {
                s = eatspace(s+11, e);
                if (0==memcmp(s, "Reverse", 7)) {
                    collisions_ = COLLISION_REVERSE;
                }
                else if (0==memcmp(s, "Normal", 6)) {
                    collisions_ = COLLISION_NORMAL;
                }
            }/*
            else if (0==memcmp(s, "ScriptType:", 11)) {
                // This is the SSA script format version. ASS version is "V4.00+"
                // c.f. v4style above.
            }*/
        }
    }

    if (NULL==events||totalStyles_<1) {
        return -1;
    }

    if (PlayResX>0 && PlayResY>0) {
        playResX_ = PlayResX;
        playResY_ = PlayResY;
    }
    else if (PlayResX>0) {
        playResX_ = PlayResX;
        playResY_ = playResX_*movieResY_/movieResX_;
    }
    else if (PlayResY>0) {
        playResY_ = PlayResY;
        playResX_ = playResY_*movieResX_/movieResY_;
    }
    else {
        Style& style = styles_[default_style];
        playResY_ = 1000*style.Fontsize/TEXT_DEFAULT_DISPLAY_HEIGHT_RATIO_PER_MILLI;
        playResX_ = playResY_*movieResX_/movieResY_;
    }

    totalDialogueFields_ = 0;
    while (0==totalDialogueFields_ && begin<end) {
        char const* e = begin + 1;
        while (*e!='\r' && *e!='\n' && e<end) {
            ++e;
        }

        char const* s = begin;
        if (e[0]=='\r' && e[1]=='\n') {
            begin = e + 2;
        }
        else {
            begin = e + 1;
        }

        if (s<e && 0==memcmp(s, "Format:", 7)) {
            s += 7;
            while (totalDialogueFields_<MAX_ASS_DIALOGUE_FIELDS && s<e) {
                while (' '==*s && s<e) ++s;
                char const* e1 = s + 1;
                while (*e1!=',' && e1<e) ++e1;

                if (s<e1) {
                    dialogueFields_[totalDialogueFields_++] = mlabs::balai::CalcCRC((uint8*)s, int(e1-s));
                }

                s = e1 + 1;
            }
            assert(10==totalDialogueFields_);
        }
    }

    if (totalDialogueFields_<10) {
        // V4.0+ Layer
        // Subtitles having different layer number will be ignored during the collusion detection.
        // Higher numbered layers will be drawn over the lower numbered.
        dialogueFields_[0] = Dialogue_Layer; 
        dialogueFields_[1] = Dialogue_Start;
        dialogueFields_[2] = Dialogue_End;
        dialogueFields_[3] = Dialogue_Style;
        dialogueFields_[4] = Dialogue_Name;
        dialogueFields_[5] = Dialogue_MarginL;
        dialogueFields_[6] = Dialogue_MarginR;
        dialogueFields_[7] = Dialogue_MarginV;
        dialogueFields_[8] = Dialogue_Effect;
        dialogueFields_[9] = Dialogue_Text;
        totalDialogueFields_ = 10;
    }

    return default_style;
}
//---------------------------------------------------------------------------------------
int Subtitle::LoadStyles(char const* style, int length, ISO_639 lan)
{
    std::lock_guard<std::mutex> lock(mutex_);
    totalStyles_ = 0;
    currentExtSubtitleStreamId_ = -1;
    if (NULL!=style && length>0) {
        playResX_ = movieResX_;
        playResY_ = movieResY_;

        defaultStyle_ = ASS_Styles_(style, style+length, lan);
        if (defaultStyle_<0) {
            BL_LOG("Not a ASS subtitle!?\n");
            totalStyles_ = 1;
            defaultStyle_ = 0;
            playResX_ = movieResX_;
            playResY_ = movieResY_;
            styles_[0].Reset();
            styles_[0].Fontsize = playResY_*TEXT_DEFAULT_DISPLAY_HEIGHT_RATIO_PER_MILLI/1000;
        }

        // set default style
        FontParam font_settings;
        styles_[defaultStyle_].BuildFontParam(font_settings);
        SetFont_(font_settings, lan);
    }
    return totalStyles_;
}
//---------------------------------------------------------------------------------------
int Subtitle::Dialogue_ASS(uint8* buffer, int buffer_size, int& width, int& height,
                           SubtitleRect* rects, int max_rects,
                           char const* ass)
{
    std::lock_guard<std::mutex> lock(mutex_);
    assert(NULL!=buffer && NULL!=rects && max_rects>0);
    width = height = 0;

    SubStream::Dialogue dialogue;
    if (NULL==ass || 0!=memcmp("Dialogue:", ass, 9) ||
        !ASS_Dialogue_(dialogue, ass+9, dialogueFields_, totalDialogueFields_)) {
        return 0;
    }

    return ASS_Dialogue_Text_(buffer, buffer_size, width, height, rects, max_rects,
                              &dialogue, 1, 0, CP_UTF8);
}
//---------------------------------------------------------------------------------------
bool Subtitle::IsFinish(int subtitleId, int timestamp)
{
    if (subtitleId<MAX_EXTERNAL_SUBTITLE_STREAMS) {
        SubStream& sub = extSubtitleStreams_[subtitleId];
        return sub.NextDialogueId>=(int)sub.Dialogues.size() || timestamp>=sub.TimeEnd;
    }
    return true;
}
int Subtitle::Publish(int subtitleId, int timestamp,
                      uint8* buffer, int buffer_size, int& width, int& height,
                      SubtitleRect* rects, int max_rects)
{
    assert(NULL!=buffer && NULL!=rects && max_rects>0 && subtitleId<MAX_EXTERNAL_SUBTITLE_STREAMS);
    std::lock_guard<std::mutex> lock(mutex_);
    if (subtitleId!=currentExtSubtitleStreamId_ || subtitleId>=MAX_EXTERNAL_SUBTITLE_STREAMS ||
        NULL==buffer || buffer_size<=0 || NULL==rects || max_rects<=0) {
        return 0;
    }

    // range check
    SubStream& sub = extSubtitleStreams_[subtitleId];
    mlabs::balai::Array<SubStream::Dialogue> const& dialogues = sub.Dialogues;
    int const total_dialogues = dialogues.size();
    if (timestamp>=sub.TimeEnd || sub.NextDialogueId>=total_dialogues) {
        return 0;
    }

    // find the range we'd like to publish
    int subtitle_begin(0), subtitle_end(0);
    for (int i=sub.NextDialogueId; i<total_dialogues; ++i) {
        SubStream::Dialogue const& s = dialogues[i];
        if ((timestamp+500)<s.TimeEnd) {
            subtitle_begin = i;
            subtitle_end = subtitle_begin + 1;
            while (subtitle_end<total_dialogues && s.TimeEnd>dialogues[subtitle_end].TimeStart) {
                ++subtitle_end;
            }
            break;
        }
        else {
            subtitle_begin = subtitle_end = i + 1;
        }
    }

    // publish
    int num_rects = 0;
    if (0<=subtitle_begin && subtitle_begin<subtitle_end) {
        SubStream::Dialogue const* start = dialogues.at(subtitle_begin);
        if (sub.Format==FORMAT_ASS) {
            num_rects = ASS_Dialogue_Text_(buffer, buffer_size, width, height, rects, max_rects,
                                       start, (subtitle_end-subtitle_begin),
                                       timestamp, sub.Codepage);
        }
        else if (sub.Format==FORMAT_SRT) {
            int const limit = 3;
            if (subtitle_end>subtitle_begin+limit) {
                subtitle_end = subtitle_begin + limit;
            }
            num_rects = SRT_Dialogue_Text_(buffer, buffer_size, width, height, rects, max_rects,
                                       start, subtitle_end-subtitle_begin,
                                       timestamp, sub.Codepage);
        }
    }

    if (sub.NextDialogueId<subtitle_end)
        sub.NextDialogueId = subtitle_end;

    return num_rects;
}

}}}