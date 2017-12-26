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
 * @file    ISO639.h
 * @author  yumei chen
 * @history 2016/08/31 created
 *
 */
#ifndef ISO_639_H
#define ISO_639_H

#include "BLCRC.h"
#include "BLHashTable.h"

namespace mlabs { namespace balai {

//
// ISO 639 - Codes for the Representation of Names of Languages
// https://www.loc.gov/standards/iso639-2/php/code_list.php
//
enum ISO_639 {
    ISO_639_UNKNOWN,
    ISO_639_CHI, // chinese
    ISO_639_DAN, // danish
    ISO_639_JPN, // japanish
    ISO_639_ISL, // icelandic
    ISO_639_SPA, // spanish
    ISO_639_FRA, // french
    ISO_639_POL, // polish
    ISO_639_FIN, // finnish
    ISO_639_ENG, // english
    ISO_639_NOR, // norwegian
    ISO_639_CES, // czech
    ISO_639_NLD, // dutch
    ISO_639_SWE, // swedish
    ISO_639_ITA, // italian
    ISO_639_DEU, // german
    ISO_639_TGL, // tagalog(filipino/philipine)
    ISO_639_THA, // thai
    ISO_639_KOR, // korean
    ISO_639_VIE, // vietnamese
    ISO_639_HIN, // hindi
    ISO_639_IND, // indonesian
    ISO_639_MSA, // malay
    ISO_639_ARA, // arabic
    ISO_639_GRE, // greek
    ISO_639_RUS, // russian
    ISO_639_POR, // portuguese
    ISO_639_TUR, // turkish
    ISO_639_TOTALS
};

char const* GetNativeLanuguageUTF8(ISO_639 code);
ISO_639 Translate_ISO_639(char const* code);
ISO_639 CodePage_To_ISO_639(UINT CodePage);

//
// Map uchardet format to Windows Code page.
// to name a few...
//  if key = "UTF-8", then 65001 is returned.
//  if key = "BIG5", 950 is returned
//  ...
//  return 0 if key unknown
uint32 FindCodePage(const char* key);

}}

#endif