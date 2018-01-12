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
 * @file    Win32Font.cpp
 * @author  andre chen, andre.HL.chen@gmail.com
 * @history 2016/07/28 created
 *
 */
#include "SystemFont.h"
#include <Windows.h>

namespace mlabs { namespace balai { namespace system {

// note FW_MEDIUM(500), FW_SEMIBOLD(600) may not widely support.
inline DWORD FontWeight(uint8 bold) {
    return (0!=bold) ? FW_BOLD:FW_NORMAL;
}

// https://msdn.microsoft.com/zh-tw/library/windows/desktop/dd317756(v=vs.85).aspx
inline uint8 CodePageToCharSet(int codepage)
{
    uint8 fdwCharSet = DEFAULT_CHARSET; // ANSI_CHARSET;
    switch (codepage)
    {
    case 737:
    case 869:
    case 875:
    case 1253:
    case 10006:
    case 20423:
    case 28597:
        fdwCharSet = GREEK_CHARSET;
        break;

    case 857:
    case 1026:
    case 1254:
    case 10081:
    case 20905:
    case 28599:
        fdwCharSet = TURKISH_CHARSET;
        break;

    case 936:
    case 10008:
    case 20936:
    case 50227:
    case 50935:
    case 50936:
    case 51936:
    case 52936:
    case 54936:
        fdwCharSet = GB2312_CHARSET;
        break;

    case 950:
    case 10002:
    case 20000:
    case 20002:
    case 50229:
    case 50937:
    case 51950:
        fdwCharSet = CHINESEBIG5_CHARSET;
        break;

    case 932:
    case 10001:
    case 20290:
    case 20932:
    case 50220:
    case 50221:
    case 50222:
    case 50930:
    case 50932:
    case 50939:
    case 51932:
        fdwCharSet = SHIFTJIS_CHARSET;
        break;

    case   949:
    case 20949:
    case 51949:
        fdwCharSet = HANGEUL_CHARSET; // a Korean
        break;

    case 1361:
    case 10003:
    case 20833:
    case 50225:
    case 50933:
        fdwCharSet = JOHAB_CHARSET;
        break;

    case 708:
    case 709:
    case 710:
    case 720:
    case 864:
    case 1256:
    case 10004:
    case 20420:
    case 28596:
        fdwCharSet = ARABIC_CHARSET;
        break;

    case 1258:
        fdwCharSet = VIETNAMESE_CHARSET;
        break;

    case 862:
    case 1255:
    case 10005:
    case 20424:
    case 28598:
    case 38598:
        fdwCharSet = HEBREW_CHARSET;
        break;

    case 775:
    case 1257:
    case 28594:
        fdwCharSet = BALTIC_CHARSET;
        break;

    case 855:
    case 866:
    case 20866:
    case 20880:
        fdwCharSet = RUSSIAN_CHARSET;
        break;

    case 874:
    case 10021:
    case 20838:
        fdwCharSet = THAI_CHARSET;
        break;

    case 1250: //???
        fdwCharSet = EASTEUROPE_CHARSET;
        break;

    default:
        break;
    }
    return fdwCharSet;
}

inline uint8 ISO639ToCharSet(ISO_639 lan) {
    //
    // DEFAULT_CHARSET set to a value based on the current system locale
    // To ensure consistent results when creating a font, do not specify
    // OEM_CHARSET or DEFAULT_CHARSET.
    // If you specify a typeface name in the lpszFace parameter,
    // make sure that the fdwCharSet value matches the character set of
    // the typeface specified in lpszFace.
    //
    // https://msdn.microsoft.com/en-us/library/cc250412.aspx
    //
    // not using...
    // HEBREW_CHARSET = 0x000000B1,
    // BALTIC_CHARSET = 0x000000BA,
    // EASTEUROPE_CHARSET = 0x000000EE,
    //
    uint8 fdwCharSet = DEFAULT_CHARSET;
    if (lan<ISO_639_TOTALS) {
        uint8 const charsets[ISO_639_TOTALS] = {
            DEFAULT_CHARSET,     // ISO_639_UNKNOW
            CHINESEBIG5_CHARSET, // ISO_639_CHI -- or GB2312_CHARSET?
            ANSI_CHARSET,        // ISO_639_DAN
            SHIFTJIS_CHARSET,    // ISO_639_JPN
            ANSI_CHARSET,        // ISO_639_ISL
            ANSI_CHARSET,        // ISO_639_SPA
            ANSI_CHARSET,        // ISO_639_FRA
            ANSI_CHARSET,        // ISO_639_POL
            ANSI_CHARSET,        // ISO_639_FIN
            ANSI_CHARSET,        // ISO_639_ENG
            ANSI_CHARSET,        // ISO_639_NOR
            ANSI_CHARSET,        // ISO_639_CES
            ANSI_CHARSET,        // ISO_639_NLD
            ANSI_CHARSET,        // ISO_639_SWE
            ANSI_CHARSET,        // ISO_639_ITA
            ANSI_CHARSET,        // ISO_639_DEU
            ANSI_CHARSET,        // ISO_639_TGL
            THAI_CHARSET,        // ISO_639_THA
            JOHAB_CHARSET,       // ISO_639_KOR --- or HANGEUL_CHARSET?
            VIETNAMESE_CHARSET,  // ISO_639_VIE

            // ISO_639_HIN Hindi, the korean character set is the most beautiful!?
            JOHAB_CHARSET,       // really???

            ANSI_CHARSET,        // ISO_639_IND
            ANSI_CHARSET,        // ISO_639_MSA
            ARABIC_CHARSET,      // ISO_639_ARA
            GREEK_CHARSET,       // ISO_639_GRE
            RUSSIAN_CHARSET,     // ISO_639_RUS
            ANSI_CHARSET,        // ISO_639_POR
            TURKISH_CHARSET,     // ISO_639_TUR
        };

        fdwCharSet = charsets[lan];
    }
    return fdwCharSet;
}

class Win32FontImp : public ISystemFont
{
    TEXTMETRIC textMetric_;
    HDC     hDC_;
    HFONT   hFont_;
    HBITMAP hBitmap_;
    HGDIOBJ hFontPrev_;
    HGDIOBJ hBitmapPrev_;
    DWORD*  bitmapPixels_;
    DWORD   charSet_;
    int     bitmapWidth_;
    int     fontHeight_; // aka bitmapHeight_

    void CopyRect_(uint8* bitmap, int width, int height, int stride) const {
        assert(NULL!=bitmap && stride>=width && width>0 && height==fontHeight_);
        for (int y=0; y<height; ++y) {
            DWORD const* src = bitmapPixels_ + y*bitmapWidth_;
            uint8* dst =  bitmap + y*stride;
            for (int x=0; x<width; ++x) {
                // pixel values are gradient, [0, 255] not just { 0 , 255 }
                *dst++ = (uint8) ((*src++) & 0xff);
                //*dst++ = (0!=(*src++)) ? 255:0; NO!
            }
        }
    }

    bool Initialize_(int font_height, char const* fontname, bool bold, DWORD fdwCharSet) {
        Finalize();
        hDC_ = CreateCompatibleDC(NULL);
        if (NULL==hDC_)
            return false;

        SetMapMode(hDC_, MM_TEXT);

        // LOGPIXELSY = Number of pixels per logical inch along the screen height.
        //font_height = MulDiv(font_height, GetDeviceCaps(hDC_, LOGPIXELSY), 72);
        hFont_ = CreateFontA(
                    font_height, // The height, in logical units The font mapper transforms this value into device units and matches its absolute value against the character height of the available fonts.
                    0,  // average width, in logical units. set 0 for closest match
                    0,  // angle, in tenths of degrees, between the escapement vector and the x-axis of the device
                    0,  // angle, in tenths of degrees, between each character's base line and the x-axis of the device.
                    FontWeight(bold), // weight range 0 through 1000. 400 is normal and 700 is bold
                    FALSE,            // italic
                    FALSE,            // underline
                    FALSE,            // strikeout
                    fdwCharSet, 
                    OUT_DEFAULT_PRECIS,  // The output precision defines how closely the output must match the requested
                    CLIP_DEFAULT_PRECIS, // The clipping precision defines how to clip characters that are partially outside the clipping region
                    ANTIALIASED_QUALITY, // we might get an anti-aliased font, no guaranteed.
                    FF_DONTCARE | VARIABLE_PITCH, // The pitch and family of the font(pitch refers to the number of characters printed per inch?)
                    fontname); // if set NULL, GDI uses the first font that matches the other specified attributes.

        if (NULL==hFont_) { // play safe
            hFont_ = CreateFontA(font_height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             fdwCharSet, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             DEFAULT_QUALITY, // Appearance of the font does not matter
                             FF_DONTCARE | DEFAULT_PITCH,
                             NULL); // default font name

            if (NULL==hFont_) {
                DeleteDC(hDC_);
                return false;
            }
        }

        hFontPrev_ = SelectObject(hDC_, hFont_);

        // create a device independent(DI) bitmap that can contain any character

        SIZE size;
        if ((0!=GetTextExtentPoint32W(hDC_, L"WWW", 3, &size))) {
            //if (font_height<size.cy)
                font_height = size.cy; // could be rounded to near size.
        }

        bitmapWidth_ = font_height*MIN_LENGTH;
        fontHeight_ = font_height;

        BITMAPINFO bmi = { 0 };
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = bitmapWidth_;
        bmi.bmiHeader.biHeight      = -font_height; // value must be negative.
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biCompression = BI_RGB;
        bmi.bmiHeader.biBitCount    = 32;
        bitmapPixels_ = NULL;
        hBitmap_ = CreateDIBSection(hDC_, &bmi, DIB_RGB_COLORS, (void**)&bitmapPixels_, NULL, 0);
        if (NULL==hBitmap_||NULL==bitmapPixels_) {
            SelectObject(hDC_, hFontPrev_);
            DeleteObject(hFont_);
            DeleteDC(hDC_);
            bitmapWidth_ = fontHeight_ = 0;
            return false;
        }

        // select objects(save old one)
        hBitmapPrev_ = SelectObject(hDC_, hBitmap_);

        // Set text properties and we are ready to go
        SetTextColor(hDC_, RGB(255,255,255));
        SetBkColor(hDC_, 0x00000000);
        SetTextAlign(hDC_, TA_TOP);

        if (!GetTextMetrics(hDC_, &textMetric_)) {
            memset(&textMetric_, 0, sizeof(textMetric_));
        }

        return true;
    }

    bool Initialize_(FontParam const& param, DWORD fdwCharSet) {
        Finalize();
        hDC_ = CreateCompatibleDC(NULL);
        if (NULL==hDC_)
            return false;

        SetMapMode(hDC_, MM_TEXT);

        // The length of this string must not exceed 32 characters, including the terminating null character
        wchar_t wfontname[32];
        int chars = MultiByteToWideChar(CP_UTF8, 0, param.name, -1, wfontname, 32);
        wfontname[chars] = 0;

        hFont_ = CreateFontW(param.size,
                    0,
                    0,
                    0,
                    FontWeight(param.bold),
                    param.italic,
                    param.underline,
                    param.strikeout,
                    fdwCharSet, 
                    OUT_DEFAULT_PRECIS,  // The output precision defines how closely the output must match the requested
                    CLIP_DEFAULT_PRECIS, // The clipping precision defines how to clip characters that are partially outside the clipping region
                    ANTIALIASED_QUALITY, // we might get an anti-aliased font, no guaranteed.
                    FF_DONTCARE | VARIABLE_PITCH, // The pitch and family of the font(pitch refers to the number of characters printed per inch?)
                    wfontname); // if set NULL, GDI uses the first font that matches the other specified attributes.

        if (NULL==hFont_) { // play safe
            return false;
        }

        hFontPrev_ = SelectObject(hDC_, hFont_);

        // create a device independent(DI) bitmap that can contain any character
        fontHeight_ = param.size;
        SIZE size;
        if ((0!=GetTextExtentPoint32W(hDC_, L"WWW", 3, &size))) {
            //if (font_height<size.cy)
                fontHeight_ = size.cy; // could be rounded to near size.
        }

        bitmapWidth_ = fontHeight_*MIN_LENGTH;

        BITMAPINFO bmi = { 0 };
        bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = bitmapWidth_;
        bmi.bmiHeader.biHeight      = -fontHeight_; // value must be negative.
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biCompression = BI_RGB;
        bmi.bmiHeader.biBitCount    = 32;
        bitmapPixels_ = NULL;
        hBitmap_ = CreateDIBSection(hDC_, &bmi, DIB_RGB_COLORS, (void**)&bitmapPixels_, NULL, 0);
        if (NULL==hBitmap_||NULL==bitmapPixels_) {
            SelectObject(hDC_, hFontPrev_);
            DeleteObject(hFont_);
            DeleteDC(hDC_);
            bitmapWidth_ = fontHeight_ = 0;
            return false;
        }

        // select objects(save old one)
        hBitmapPrev_ = SelectObject(hDC_, hBitmap_);

        // Set text properties and we are ready to go
        SetTextColor(hDC_, RGB(255,255,255));
        SetBkColor(hDC_, 0x00000000);
        SetTextAlign(hDC_, TA_TOP);

        if (!GetTextMetrics(hDC_, &textMetric_)) {
            memset(&textMetric_, 0, sizeof(textMetric_));
        }

        return true;
    }

public:
    Win32FontImp():textMetric_(),hDC_(NULL),hFont_(NULL),hBitmap_(NULL),hFontPrev_(NULL),
        hBitmapPrev_(NULL),bitmapPixels_(NULL),
        charSet_(0),bitmapWidth_(0),fontHeight_(0) {
        memset(&textMetric_, 0, sizeof(textMetric_));
    }
    ~Win32FontImp() { Finalize(); }

    //
    // Don't DO This!!!
    //
    // LOGFONTA lf;
    // if (NULL==name && GetObjectA(GetStockObject(SYSTEM_FONT), sizeof(LOGFONTA), &lf)) {
    //  name = lf.lfFaceName;
    // }
    //
    bool Initialize(int font_height, char const* name, bool bold, int codepage) {
        DWORD const charset = (codepage!=0 || 0==charSet_) ? CodePageToCharSet(codepage):charSet_;
        if (Initialize_(font_height, name, bold, charset)) {
            charSet_ = charset;
            return true;
        }
        return false;
    }
    bool Initialize(FontParam const& param, int codepage) {
        DWORD const charset = (codepage!=0 || 0==charSet_) ? CodePageToCharSet(codepage):charSet_;
        if (Initialize_(param, charset)) {
            charSet_ = charset;
            return true;
        }
        return false;
    }

    bool Initialize(int font_height, char const* name, bool bold, ISO_639 lan) {
        DWORD const charset = (ISO_639_UNKNOWN!=lan || 0==charSet_) ? ISO639ToCharSet(lan):charSet_;
        if (Initialize_(font_height, name, bold, charset)) {
            charSet_ = charset;
            return true;
        }
        return false;
    }
    bool Initialize(FontParam const& param, ISO_639 lan) {
        DWORD const charset = (ISO_639_UNKNOWN!=lan || 0==charSet_) ? ISO639ToCharSet(lan):charSet_;
        if (Initialize_(param, charset)) {
            charSet_ = charset;
            return true;
        }
        return false;
    }

    void Finalize() {
        memset(&textMetric_, 0, sizeof(textMetric_));
        if (hDC_) {
            if (hBitmapPrev_) SelectObject(hDC_, hBitmapPrev_);
            if (hFontPrev_) SelectObject(hDC_, hFontPrev_);
            if (hFont_) DeleteObject(hFont_);
            if (hBitmap_) DeleteObject(hBitmap_);
            DeleteDC(hDC_);
            hDC_ = NULL;
        }
        bitmapPixels_ = NULL;
        bitmapWidth_ = fontHeight_ = 0;
    }

    bool GetMaxTextExtend(int& width, int& height) const {
        if (hDC_) {
            width = bitmapWidth_;
            height = fontHeight_;
            return width>0 && height>0;
        }
        return false;
    }

    bool GetTextExtend(int& width, int& height, wchar_t const* text, int len) const {
        //
        // "Calculating Text Extents of Bold and Italic Text"
        // https://support.microsoft.com/en-us/kb/74298
        //
        // Note : GetTextExtentPoint32W() does not give you the extent of specified text.
        //        Instead, it gives you the next text position after thxt.
        //
        SIZE size;
        if (hDC_ && text && len>0 && (0!=GetTextExtentPoint32W(hDC_, text, len, &size))) {
            //
            // The TrueType rasterizer provides ABC character spacing after a 
            // specific point size has been selected.
            //   A spacing is the distance added to the current position before placing the glyph.
            //   B spacing is the width of the black part of the glyph.
            //   C spacing is the distance added to the current position to provide white space
            //     to the right of the glyph.
            //   The total advanced width is specified by A+B+C.
            //
            if (FW_NORMAL<textMetric_.tmWeight || textMetric_.tmItalic) {
                //
                // When the GetCharABCWidths function retrieves negative A or C widths for a character,
                // that character includes underhangs or overhangs.
                //
                ABC abc;
                UINT const ch = text[len-1];
                if (GetCharABCWidthsW(hDC_, ch, ch, &abc)) {
                    if (abc.abcC<0) { // overhangs
                        if (textMetric_.tmItalic) {
                            //
                            // is it truly safe!?
                            //
                            size.cx -= abc.abcC;
                        }
                        else if (-abc.abcC<3) {
                            //
                            // The overhang for bold characters synthesized by GDI is
                            // generally 1 because GDI synthesizes bold fonts by
                            // outputting the text twice, offsetting the second output
                            // by one pixel, effectively increasing the width of each
                            // character by one pixel.
                            //
                            size.cx -= abc.abcC;
                        }
                    }
                }
            }
            width = size.cx;
            height = size.cy;
            return (width<=bitmapWidth_);
        }
        width = height = 0;
        return false;
    }

    bool GetTextExtend(int& width, int& height, char const* text, int len) const {
        wchar_t wtext[MIN_LENGTH];
        len = MultiByteToWideChar(CP_UTF8, 0, text, len, wtext, MIN_LENGTH);
        return GetTextExtend(width, height, wtext, len);
    }

    bool BlitText(uint8* glyph, int stride, int width, int height, wchar_t const* text, int len) const {
        if (glyph && hDC_ && text && len>0 && width<=stride &&
            ExtTextOutW(hDC_, 0, 0, ETO_OPAQUE, NULL, text, (UINT) len, NULL)) {
            CopyRect_(glyph, width, height, stride);
            return true;
        }
        return false;
    }

    bool BlitText(uint8* glyph, int stride, int width, int height, char const* text, int len) const {
        wchar_t wtext[MIN_LENGTH];
        len = MultiByteToWideChar(CP_UTF8, 0, text, len, wtext, MIN_LENGTH);
        return BlitText(glyph, stride, width, height, wtext, len);
    }

    uint8 const* GetTextRect(int& width, int& height, wchar_t const* text, int len) const {
        if (NULL!=text && len>0 &&
            GetTextExtend(width, height, text, len) &&
            ExtTextOutW(hDC_, 0, 0, ETO_OPAQUE, NULL, text, (UINT) len, NULL)) {
            uint8* dst = (uint8*) bitmapPixels_; 
            for (int y=0; y<height; ++y) {
                DWORD const* src = bitmapPixels_ + y*bitmapWidth_;
                for (int x=0; x<width; ++x) {
                    // pixel values are gradient, [0, 255] not just { 0, 255 }
                    *dst++ = (uint8) ((*src++) & 0xff);
                    //*dst++ = (0!=(*src++)) ? 255:0; NO!
                }
            }
            return (uint8*) bitmapPixels_;
        }
        return NULL;
    }

    uint8 const* GetTextRect(int& width, int& height, char const* text, int len) const {
        wchar_t wtext[MIN_LENGTH];
        len = MultiByteToWideChar(CP_UTF8, 0, text, len, wtext, MIN_LENGTH);
        return GetTextRect(width, height, wtext, len);
    }
};

//---------------------------------------------------------------------------------------
ISystemFont* ISystemFont::New() {
    try {
        return new Win32FontImp();
    }
    catch (...) {
        return NULL;
    }
}

struct EnumFontParam {
    LONG Founds;
    LONG Weight; // 400~700
    BYTE Italic;
    BYTE Underline;
    BYTE StrikeOut;
    BYTE CharSet;
};

// enumerate font callback... note CALLBACK may be different for win32/x64
int __stdcall FontEnumProc(LOGFONTW const* f, TEXTMETRICW const* m, DWORD FontType, LPARAM lParam) {
    if (f && m && lParam && 0!=(FontType&TRUETYPE_FONTTYPE)) {
        EnumFontParam* param = (EnumFontParam*) lParam;
#if 0
        OutputDebugStringW(f->lfFaceName);
        BL_LOG("  type:%d CharSet:%d(%d) Weight:%d Italic:%d Underline:%d StrikeOut:%d\n", FontType,
                f->lfCharSet, param->CharSet,
                f->lfWeight, f->lfItalic, f->lfUnderline, f->lfStrikeOut);
#endif
        if (f->lfWeight==param->Weight &&
            f->lfItalic==param->Italic &&
            f->lfUnderline==param->Underline &&
            f->lfStrikeOut==param->StrikeOut &&
            (f->lfCharSet==param->CharSet || 0==param->CharSet)) {
            ++(param->Founds);
            return 0; // stop enumeration
        }
    }
    return 1;
}

bool IsFontAvailable(FontParam const& font, ISO_639 lan, int codepage)
{
    LOGFONTW logFont; memset(&logFont, 0, sizeof(logFont));
    int len = MultiByteToWideChar(CP_UTF8, 0, font.name, -1, logFont.lfFaceName, LF_FACESIZE);
    if (0<len && len<LF_FACESIZE) {
        // EnumFontFamiliesExW examines the following 3 members
        //
        // [lfCharSet] If set to DEFAULT_CHARSET, the function enumerates all uniquely-named
        // fonts in all character sets. (If there are two fonts with the same name, only one is enumerated.)
        // If set to a valid character set value, the function enumerates only fonts in the specified character set.
        logFont.lfFaceName[len] = L'\0';
        logFont.lfCharSet = (CP_UTF8==codepage) ? ISO639ToCharSet(lan):CodePageToCharSet(codepage);
        logFont.lfPitchAndFamily = 0; // must be 0

        EnumFontParam enumFont;
        enumFont.Founds = 0;
        enumFont.CharSet = (DEFAULT_CHARSET==logFont.lfCharSet) ? 0:logFont.lfCharSet;
        enumFont.Weight = FontWeight(font.bold);
        enumFont.Italic = font.italic;
        enumFont.Underline = font.underline;
        enumFont.StrikeOut = font.strikeout;

        HDC hDC = CreateCompatibleDC(NULL);
        EnumFontFamiliesExW(hDC, &logFont, FontEnumProc, (LPARAM) &enumFont, 0);
        DeleteDC(hDC);

        return (enumFont.Founds>0);
    }
    return false;
}

}}}