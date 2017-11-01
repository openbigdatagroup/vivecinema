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
 * @file    ISO639.cpp
 * @author  yumei chen
 * @history 2016/08/31 created
 *
 */

#include "ISO639.h"

namespace mlabs { namespace balai {

static const char* languageUTF8[ISO_639_TOTALS] = {
    "????",
    "\xe4\xb8\xad\xe6\x96\x87",
    "\x64\x61\x6e\x73\x6b",
    "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e",
    "\xc3\x8d\x73\x6c\x65\x6e\x73\x6b\x61",
    "\x65\x73\x70\x61\xc3\xb1\x6f\x6c",
    "\x66\x72\x61\x6e\xc3\xa7\x61\x69\x73",
    "\x6a\xc4\x99\x7a\x79\x6b\x20\x70\x6f\x6c\x73\x6b\x69",
    "\x73\x75\x6f\x6d\x69",
    "\x45\x6e\x67\x6c\x69\x73\x68",
    "\x4e\x6f\x72\x73\x6b",
    "\xc4\x8d\x65\xc5\xa1\x74\x69\x6e\x61",
    "\x4e\x65\x64\x65\x72\x6c\x61\x6e\x64\x73",
    "\x73\x76\x65\x6e\x73\x6b\x61",
    "\x69\x74\x61\x6c\x69\x61\x6e\x6f",
    "\x44\x65\x75\x74\x73\x63\x68",
    "\x57\x69\x6b\x61\x6e\x67\x20\x54\x61\x67\x61\x6c\x6f\x67",
    "\xe0\xb9\x84\xe0\xb8\x97\xe0\xb8\xa2",
    "\xed\x95\x9c\xea\xb5\xad\xec\x96\xb4",
    "\x54\x69\xe1\xba\xbf\x6e\x67\x20\x56\x69\xe1\xbb\x87\x74",
    "\xe0\xa4\xb9\xe0\xa4\xbf\xe0\xa4\xa8\xe0\xa5\x8d\xe0\xa4\xa6\xe0\xa5\x80",
    "\x42\x61\x68\x61\x73\x61\x20\x49\x6e\x64\x6f\x6e\x65\x73\x69\x61",
    "\x62\x61\x68\x61\x73\x61\x20\x4d\x65\x6c\x61\x79\x75",
    "\xd8\xa7\xd9\x84\xd8\xb9\xd8\xb1\xd8\xa8\xd9\x8a\xd8\xa9",
    "\xce\xb5\xce\xbb\xce\xbb\xce\xb7\xce\xbd\xce\xb9\xce\xba\xce\xac",
    "\xd0\xa0\xd1\x83\xd1\x81\xd1\x81\xd0\xba\xd0\xb8\xd0\xb9",
    "\x70\x6f\x72\x74\x75\x67\x75\xc3\xaa\x73",
    "\x54\xc3\xbc\x72\x6b\xc3\xa7\x65",
};

#define PACK_3CC(a, b, c) ((int(a)<<16) | (int(b)<<8) | (int(c)))
int const iso639code[ISO_639_TOTALS][3] = {
    { PACK_3CC('u', 'n', 'd'), -1, -1 },
    { PACK_3CC('c', 'h', 'i'), PACK_3CC('z', 'h', 'o'), -1 },
    { PACK_3CC('d', 'a', 'n'), -1, -1 },
    { PACK_3CC('j', 'p', 'n'), PACK_3CC('j', 'a', '\0'), -1 },
    { PACK_3CC('i', 's', 'l'), PACK_3CC('i', 'c', 'e'), PACK_3CC('i', 's', '\0') },
    { PACK_3CC('s', 'p', 'a'), PACK_3CC('e', 's', '\0'), -1 },
    { PACK_3CC('f', 'r', 'a'), PACK_3CC('f', 'r', 'e'), PACK_3CC('f', 'r', '\0') },
    { PACK_3CC('p', 'o', 'l'), PACK_3CC('p', 'l', '\0'), -1 },
    { PACK_3CC('f', 'i', 'n'), PACK_3CC('f', 'i', '\0'), -1 },
    { PACK_3CC('e', 'n', 'g'), PACK_3CC('e', 'n', '\0'), -1 },
    { PACK_3CC('n', 'o', 'r'), PACK_3CC('n', 'o', 'b'), PACK_3CC('n', 'n', 'o') },
    { PACK_3CC('c', 'e', 's'), PACK_3CC('c', 'z', 'e'), PACK_3CC('c', 's', '\0') },
    { PACK_3CC('n', 'l', 'd'), PACK_3CC('d', 'u', 't'), PACK_3CC('n', 'l', '\0') },
    { PACK_3CC('s', 'w', 'e'), PACK_3CC('s', 'v', '\0'), -1 },
    { PACK_3CC('i', 't', 'a'), PACK_3CC('i', 't', '\0'), -1 },
    { PACK_3CC('d', 'e', 'u'), PACK_3CC('g', 'e', 'r'), PACK_3CC('d', 'e', '\0') },
    { PACK_3CC('t', 'g', 'l'), PACK_3CC('t', 'l', '\0'), -1 },
    { PACK_3CC('t', 'h', 'a'), PACK_3CC('t', 'h', '\0'), -1 },
    { PACK_3CC('k', 'o', 'r'), PACK_3CC('k', 'o', '\0'), -1 },
    { PACK_3CC('v', 'i', 'e'), PACK_3CC('v', 'i', '\0'), -1 },
    { PACK_3CC('h', 'i', 'n'), PACK_3CC('h', 'i', '\0'), -1 },
    { PACK_3CC('i', 'n', 'd'), PACK_3CC('i', 'd', '\0'), -1 },
    { PACK_3CC('m', 's', 'a'), PACK_3CC('m', 'a', 'y'), PACK_3CC('m', 's', '\0') },
    { PACK_3CC('a', 'r', 'a'), PACK_3CC('a', 'r', '\0'), -1 },
    { PACK_3CC('e', 'l', 'l'), PACK_3CC('g', 'r', 'e'), PACK_3CC('e', 'l', '\0') },
    { PACK_3CC('r', 'u', 's'), PACK_3CC('r', 'u', '\0'), -1 },
    { PACK_3CC('p', 'o', 'r'), PACK_3CC('p', 't', '\0'), -1 },
    { PACK_3CC('t', 'u', 'r'), PACK_3CC('t', 'r', '\0'), -1 },
};

char const* GetNativeLanuguageUTF8(ISO_639 type) {
    return languageUTF8[type<ISO_639_TOTALS ? type:0];
}

ISO_639 Translate_ISO_639(char const* c) {
    if (NULL!=c && '\0'==c[3]) {
        int const _3cc = PACK_3CC(c[0], c[1], c[2]);
        for (int i=0; i<ISO_639_TOTALS; ++i) {
            if (_3cc==iso639code[i][0] || _3cc==iso639code[i][1] || _3cc==iso639code[i][2])
                return (ISO_639) i;
        }

        if (_3cc==PACK_3CC('n', 'o', '\0') ||
            _3cc==PACK_3CC('n', 'b', '\0') ||
            _3cc==PACK_3CC('n', 'n', '\0')) {
            return ISO_639_NOR;
        }
    }
    return ISO_639_UNKNOWN;
}

ISO_639 CodePage_To_ISO_639(UINT CodePage) {
    ISO_639 ans(ISO_639_UNKNOWN);
    switch(CodePage) {
    // International
    case 65001: // UTF-8
    case 1200:  // UTF-16LE
    case 1201:  // UTF-16BE
    case 12000: // UTF-32LE
    case 12001: // UTF-32BE
        break;

    // Arabic
    case 28596: // ISO-8859-6
    case 1256:  // WINDOWS-1256
        ans = ISO_639_ARA;
        break;
        
    //// Blugarian
    //case 28595: // ISO-8859-5
    //case 1251:  // WINDOWS-1251
    //    // Not Support yet!
    //    break;

    // Chinese
    case 950:   // BIG5
    case 54936: // GB18030
    case 52936: //HZ-GB-2312
        ans = ISO_639_CHI;
        break;

    // Danish(28591, 28605, 1252), French(28591, 28605, 1252), German(28591, 1252), Spanish(28591, 28605, 1252)
    case 28591: // ISO-8859-1
    case 28605: // ISO-8859-15
    case 1252:  // WINDOWS-1252
        ans = ISO_639_DAN;
        break;

    // English
    case 20127: // ASCII
        ans = ISO_639_ENG;
        break;

    //// Esperanto
    //case 28593: // ISO-8859-3
    //    // Not Support yet!
    //    break;

    // Greek
    case 28597: // ISO-8859-7
    case 1253:  // WINDOWS-1253
        ans = ISO_639_GRE;
        break;

    // Hebrew
    case 28598: // ISO-8859-8
    case 1255:  // WINDOWS-1255
        // Not support yet!
        break;

    // Hungarian
    case 28592: // ISO-8859-2
    case 1250:  // WINDOWS-1250
        // Not support yet!
        break;

    // Japanese
    case 50220: // ISO-2022-JP
    case 932:   // SHIFT_JIS
    case 20932: // EUC-JP
        ans = ISO_639_JPN;
        break;

    // Korean
    case 50225: // ISO-2022-KR
    case 51949: // EUC-KR
        ans = ISO_639_KOR;
        break;

    // Russian
    case 28595: // ISO-8859-5 ( the same as Blugarian)
    case 1251:  //WINDOWS-1251 ( the same as Blugarian)
    case 20866: // KOI8-R
    case 10007: // MAC-CYRILLIC
    case 855:   // IBM855
        ans = ISO_639_RUS;
        break;

    // Thai
    case 874:   // ISO-8859-11
        ans = ISO_639_THA;
        break;

    // Turkish
    case 28593: // ISO-8859-3   ( the same as Esperanto)
    case 28599: // ISO-8859-9
        ans = ISO_639_TUR;
        break;

    // Vietnamese
    case 1258:  // WINDOWS-1258
        ans = ISO_639_VIE;
        break;

    // Others
        // hashtable.insert(MACRO_CRC("WINDOWS-1252"), 1252); // Repeat to Danishn
    default:
        break;
    }
    return ans;
}


//
// uchar det format: https://github.com/BYVoid/uchardet
// Windows Code Page: https://msdn.microsoft.com/en-us/library/windows/desktop/dd317756(v=vs.85).aspx
//
class UchardetHashTable
{
    mlabs::balai::HashTable<uint32, uint32, 64> hashtable;

public:
#define MACRO_CRC(str) mlabs::balai::CalcWordCRC((##str))
    UchardetHashTable() {
    // International (Unicode)
        hashtable.insert(MACRO_CRC("UTF-8"), 65001);
        hashtable.insert(MACRO_CRC("UTF-16LE"), 1200);
        hashtable.insert(MACRO_CRC("UTF-16BE"), 1201);
        hashtable.insert(MACRO_CRC("UTF-32LE"), 12000);
        hashtable.insert(MACRO_CRC("UTF-32BE"), 12001);
        // hashtable.insert(MACRO_CRC("X-ISO-10646-UCS-4-34121"), );
        // hashtable.insert(MACRO_CRC("X-ISO-10646-UCS-4-21431"), );
    // Arabic
        hashtable.insert(MACRO_CRC("ISO-8859-6"), 28596);
        hashtable.insert(MACRO_CRC("WINDOWS-1256"), 1256);
    // Blugarian
        hashtable.insert(MACRO_CRC("ISO-8859-5"), 28595);
        hashtable.insert(MACRO_CRC("WINDOWS-1251"), 1251);
    // Chinese
        // hashtable.insert(MACRO_CRC("ISO-2022-CN"), );
        hashtable.insert(MACRO_CRC("BIG5"), 950);
        // hashtable.insert(MACRO_CRC("EUC-TW"), );
        hashtable.insert(MACRO_CRC("GB18030"), 54936);
        hashtable.insert(MACRO_CRC("HZ-GB-2312"), 52936);
    // Danish
        hashtable.insert(MACRO_CRC("ISO-8859-1"), 28591);
        hashtable.insert(MACRO_CRC("ISO-8859-15"), 28605);
        hashtable.insert(MACRO_CRC("WINDOWS-1252"), 1252);
    // English
        hashtable.insert(MACRO_CRC("ASCII"), 20127);
    // Esperanto
        hashtable.insert(MACRO_CRC("ISO-8859-3"), 28593);
    // French
        // hashtable.insert(MACRO_CRC("ISO-8859-1"), 28591); // Repeat to Danish
        // hashtable.insert(MACRO_CRC("ISO-8859-15"), 28605); // Repeat to Danish
        // hashtable.insert(MACRO_CRC("WINDOWS-1252"), 1252); // Repeat to Danish
    // German
        // hashtable.insert(MACRO_CRC("ISO-8859-1"), 28591); // Repeat to Danish
        // hashtable.insert(MACRO_CRC("WINDOWS-1252"), 1252); // Repeat to Danish
    // Greek
        hashtable.insert(MACRO_CRC("ISO-8859-7"), 28597);
        hashtable.insert(MACRO_CRC("WINDOWS-1253"), 1253);
    // Hebrew
        hashtable.insert(MACRO_CRC("ISO-8859-8"), 28598);
        hashtable.insert(MACRO_CRC("WINDOWS-1255"), 1255);
    // Hungarian
        hashtable.insert(MACRO_CRC("ISO-8859-2"), 28592);
        hashtable.insert(MACRO_CRC("WINDOWS-1250"), 1250);
    // Japanese
        hashtable.insert(MACRO_CRC("ISO-2022-JP"), 50220);
        hashtable.insert(MACRO_CRC("SHIFT_JIS"), 932);
        hashtable.insert(MACRO_CRC("EUC-JP"), 20932);   // or 51932, but 51932 doesn't work.....
    // Korean
        hashtable.insert(MACRO_CRC("ISO-2022-KR"), 50225);
        hashtable.insert(MACRO_CRC("EUC-KR"), 51949);
    // Russian
        // hashtable.insert(MACRO_CRC("ISO-8859-5"), 28595); // Repeat to Blugarian
        hashtable.insert(MACRO_CRC("KOI8-R"), 20866);
        // hashtable.insert(MACRO_CRC("WINDOWS-1251"), 1251); // Repeat to Blugarian
        hashtable.insert(MACRO_CRC("MAC-CYRILLIC"), 10007);
        // hashtable.insert(MACRO_CRC("IBM866"), );
        hashtable.insert(MACRO_CRC("IBM855"), 855);
    // Spanish
        // hashtable.insert(MACRO_CRC("ISO-8859-1"), 28591); // Repeat to Danish
        // hashtable.insert(MACRO_CRC("ISO-8859-15"), 28605); // Repeat to Danish
        // hashtable.insert(MACRO_CRC("WINDOWS-1252"), 1252); // Repeat to Danish
    // Thai
        // hashtable.insert(MACRO_CRC("TIS-620"), );
        hashtable.insert(MACRO_CRC("ISO-8859-11"), 874);
    // Turkish
        // hashtable.insert(MACRO_CRC("ISO-8859-3"), 28593); // Repeat to Esperanto
        hashtable.insert(MACRO_CRC("ISO-8859-9"), 28599);
    // Vietnamese
        // hashtable.insert(MACRO_CRC("VISCII"), );
        hashtable.insert(MACRO_CRC("WINDOWS-1258"), 1258);
    // Others
        // hashtable.insert(MACRO_CRC("WINDOWS-1252"), 1252); // Repeat to Danishn
    }

    ~UchardetHashTable() { hashtable.clear();}

    uint32 FindCodePage(const char* key) const {
        uint32 codepage = 0;
        return hashtable.find(MACRO_CRC(key), &codepage) ? codepage:0;
    }
#undef MACRO_CRC
};

uint32 FindCodePage(const char* key) {
    if (NULL!=key) {
        static UchardetHashTable inst_;
        return inst_.FindCodePage(key);
    }
    return 0;
}

}} // namespace mlabs::balai