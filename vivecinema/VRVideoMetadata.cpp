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
 * @file    VideoMetadata.cpp
 * @author  andre chen, andre.HL.chen@gmail.com
 * @history 2016/03/21 created
 *
 */
#include "VRVideoPlayer.h"
#include "BLCRC.h"
#include "BLXML.h"

// config file text encoding
#include "uchardet.h"

// xml
typedef mlabs::balai::fileio::XMLParserLite<> XMLParser;
using mlabs::balai::fileio::XML_Element;
using mlabs::balai::fileio::XML_Attrib;

using namespace mlabs::balai::audio;

// H.264
class BitStream {
    uint8 const* const start_;
    uint8 const* const end_;
    mutable uint8 const* ptr_;
    mutable int bits_left_;

    BL_NO_DEFAULT_CTOR(BitStream);
    BL_NO_COPY_ALLOW(BitStream);

public:
    BitStream(uint8 const* start, uint8 const* end):start_(start),end_(end),
        ptr_(start),bits_left_(8) {}
    ~BitStream() {}

    bool end() const { return ptr_>=end_; }
    //uint32 read_byte(int n) const { return read_bits(8*n); }
    uint32 read_bits(int n) const {
        assert(0<n && n<=32);
        uint32 r = 0;
        for (int i=0; i<n; ++i) {
            r = (r<<1) | read_u1();
        }
        return r;
    }
    uint32 read_u1() const {
        if (ptr_<end_) {
            uint32 const r = ((*ptr_)>>(--bits_left_)) & 0x01;
            if (0==bits_left_) {
                bits_left_ = 8;
                ++ptr_;
            }
            return r;
        }
        return 0;
    }
    uint32 read_ue() const {
        int n = 0; // leading zero bits
        while (ptr_<end_ && 0==read_u1() && n<32) {
            ++n;
        }
        return (n>0) ? (((1<<n) - 1) + read_bits(n)):0;
    }
};

inline bool annexB_startcode(uint8 const* ptr) {
    return 0x00==ptr[0] && 0x00==ptr[1] && (0x01==ptr[2] || (0x00==ptr[2] && 0x01==ptr[3]));
}
inline bool sei_message(uint32& payloadType, uint32& payloadSize,
                        uint8 const*& ptr, uint8 const* const end) {
    payloadType = payloadSize = 0;
    while (ptr<end && 0xff==*ptr) {
        payloadType += 0xff;
        ++ptr;
    }
    if (ptr>=end) return false;
    payloadType += *ptr++; // +last_payload_type_byte
    
    while (ptr<end && 0xff==*ptr) {
        payloadSize += 0xff;
        ++ptr;
    }
    if (ptr>=end) return false;
    payloadSize += *ptr++; // +last_payload_size_byte

    return true;
}

// Matroska file structure : EBML = Extensible Binary Meta Language
inline uint32 EBML_element_header(uint32& read_bytes, FILE* file) {
    uint32 header = 0;
    uint8 buf, byte;
    if (1==fread(&byte, 1, 1, file)) {
        uint8 mask = 1<<7;
        for (int i=0; i<4; ++i) {
            if (mask&byte) {
                read_bytes = i + 1;
                return header|(byte<<(i*8));
            }
            else if (1==fread(&buf, 1, 1, file)) {
                header = header<<8 | buf;
                mask >>= 1;
            }
            else {
                break;
            }
        }
    }
    return header;
}
inline uint32 EBML_element_read_uint(FILE* file, uint32 size) {
    uint8 buf[4];
    if (size<=4 && size==fread(buf, 1, size, file)) {
        uint32 res = buf[0];
        for (uint32 i=1; i<size; ++i) {
            res = (res<<8) | buf[i];
        }
        return res;
    }
    return 0;
}
inline uint64 EBML_element_size(uint32& read_bytes, FILE* file) {
    uint64 size = 0;
    uint8 buf, byte;
    if (1==fread(&byte, 1, 1, file)) {
        uint8 mask = 1<<7;
        for (int i=0; i<8; ++i) {
            if (mask&byte) {
                byte ^= mask;
                read_bytes = i+1;
                return size|(byte<<(i*8));
            }
            else if (1==fread(&buf, 1, 1, file)) {
                size = size<<8 | buf;
                mask >>= 1;
            }
            else {
                break;
            }
        }
    }
    return size;
}

/*
a 360 degree video must have metadata in header...
https://github.com/google/spatial-media/blob/master/docs/spherical-video-rfc.md

may have xml header: <?xml version="1.0"?>

// Theta stitcher
<rdf:SphericalVideo xmlns:GSpherical='http://ns.google.com/videos/1.0/spherical/'
                    xmlns:rdf='http://www.w3.org/1999/02/22-rdf-syntax-ns#'>
  <GSpherical:Spherical>true</GSpherical:Spherical>
  <GSpherical:Stitched>true</GSpherical:Stitched>
  <GSpherical:ProjectionType>equirectangular</GSpherical:ProjectionType>
  <GSpherical:StitchingSoftware>RICOH THETA::DualfishBlender.exe 1.1.1.2015.09.29</GSpherical:StitchingSoftware>
  <GSpherical:SourceCount>1</GSpherical:SourceCount>
  <GSpherical:Timestamp>3533202402</GSpherical:Timestamp>
</rdf:SphericalVideo>

// a jump vr example - GSpherical:StereoMode
// https://www.google.com/get/cardboard/jump/
<rdf:SphericalVideo xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#"
                    xmlns:GSpherical="http://ns.google.com/videos/1.0/spherical/">
  <GSpherical:Spherical>true</GSpherical:Spherical>
  <GSpherical:Stitched>true</GSpherical:Stitched>
  <GSpherical:StitchingSoftware>Spherical Metadata Tool</GSpherical:StitchingSoftware>
  <GSpherical:ProjectionType>equirectangular</GSpherical:ProjectionType>
  <GSpherical:StereoMode>top-bottom</GSpherical:StereoMode>
</rdf:SphericalVideo>

<rdf:SphericalVideo xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#"
                    xmlns:GSpherical="http://ns.google.com/videos/1.0/spherical/">
  <GSpherical:Spherical>true</GSpherical:Spherical>
  <GSpherical:Stitched>true</GSpherical:Stitched>
  <GSpherical:StitchingSoftware>OpenCV for Windows v2.4.9</GSpherical:StitchingSoftware>
  <GSpherical:ProjectionType>equirectangular</GSpherical:ProjectionType>
  <GSpherical:SourceCount>6</GSpherical:SourceCount>
  <GSpherical:InitialViewHeadingDegrees>90</GSpherical:InitialViewHeadingDegrees>
  <GSpherical:InitialViewPitchDegrees>0</GSpherical:InitialViewPitchDegrees>
  <GSpherical:InitialViewRollDegrees>0</GSpherical:InitialViewRollDegrees>
  <GSpherical:Timestamp>1400454971</GSpherical:Timestamp>
  <GSpherical:CroppedAreaImageWidthPixels>1920</GSpherical:CroppedAreaImageWidthPixels>
  <GSpherical:CroppedAreaImageHeightPixels>1080</GSpherical:CroppedAreaImageHeightPixels>
  <GSpherical:FullPanoWidthPixels>1900</GSpherical:FullPanoWidthPixels>
  <GSpherical:FullPanoHeightPixels>960</GSpherical:FullPanoHeightPixels>
  <GSpherical:CroppedAreaLeftPixels>15</GSpherical:CroppedAreaLeftPixels>
  <GSpherical:CroppedAreaTopPixels>60</GSpherical:CroppedAreaTopPixels>
</rdf:SphericalVideo>


// VirtuePorn(180 SBS)
<x:xmpmeta xmlns:x="adobe:ns:meta/" x:xmptk="Adobe XMP Core 5.6-c111 79.158325, 2015/09/10-01:10:20        ">
 <rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
  <rdf:Description rdf:about=""
    xmlns:xmp="http://ns.adobe.com/xap/1.0/"
    xmlns:xmpDM="http://ns.adobe.com/xmp/1.0/DynamicMedia/"
    xmlns:stDim="http://ns.adobe.com/xap/1.0/sType/Dimensions#"
    xmlns:xmpMM="http://ns.adobe.com/xap/1.0/mm/"
    xmlns:stEvt="http://ns.adobe.com/xap/1.0/sType/ResourceEvent#"
    xmlns:stRef="http://ns.adobe.com/xap/1.0/sType/ResourceRef#"
    xmlns:GPano="http://ns.google.com/photos/1.0/panorama/"
    xmlns:tiff="http://ns.adobe.com/tiff/1.0/"
    xmlns:creatorAtom="http://ns.adobe.com/creatorAtom/1.0/"
    xmlns:dc="http://purl.org/dc/elements/1.1/"
    xmlns:bext="http://ns.adobe.com/bwf/bext/1.0/"
   xmp:CreateDate="2016-05-13T16:41:30+02:00"
   xmp:ModifyDate="2016-05-13T16:41:31+02:00"
   xmp:MetadataDate="2016-05-13T16:41:31+02:00"
   xmp:CreatorTool="Adobe Premiere Pro CC (Windows)"
   xmpDM:videoFrameRate="59.940060"
   xmpDM:videoFieldOrder="Progressive"
   xmpDM:videoPixelAspectRatio="1/1"
   xmpDM:audioSampleRate="48000"
   xmpDM:audioSampleType="16Int"
   xmpDM:audioChannelType="Stereo"
   xmpDM:startTimeScale="60000"
   xmpDM:startTimeSampleSize="1001"
   xmpMM:InstanceID="xmp.iid:dd407e60-ded0-6e47-a995-10486fd7d930"
   xmpMM:DocumentID="535ed8f8-4e2f-00b8-ca28-6f430000009d"
   xmpMM:OriginalDocumentID="xmp.did:53ce07f4-7898-5f44-818c-f050a12a03f4"
   dc:format="H.264">

   ....

   <xmpMM:Pantry>
    <rdf:Bag>
     <rdf:li>
      <rdf:Description
       GPano:ProjectionType="equirectangular"
       GPano:UsePanoramaViewer="True"
       GPano:CroppedAreaImageWidthPixels="1600"
       GPano:CroppedAreaImageHeightPixels="1600"
       GPano:FullPanoWidthPixels="3200"
       GPano:FullPanoHeightPixels="1600"
       GPano:CroppedAreaLeftPixels="800"
       GPano:CroppedAreaTopPixels="0"
       GPano:StitchingSoftware="PTGui Pro 10.0.13 (www.ptgui.com)"
       tiff:XResolution="300/1"
       tiff:YResolution="300/1"
       tiff:ResolutionUnit="2"
       xmp:CreatorTool="PTGui Pro 10.0.13 (www.ptgui.com)"
       xmp:MetadataDate="2016-05-13T15:05+02:00"
       xmp:ModifyDate="2016-05-13T15:05+02:00"
       xmpMM:InstanceID="182ffcc5-cd9a-f98d-e829-fc7d0000006e"
       xmpMM:DocumentID="e23db23e-6c27-685a-3886-c6e000000041"
       xmpMM:OriginalDocumentID="xmp.did:f2485d29-09f6-214b-886b-ca559d541252">
      <xmpMM:History>
       <rdf:Seq>
        <rdf:li
         stEvt:action="saved"
         stEvt:instanceID="182ffcc5-cd9a-f98d-e829-fc7d0000006e"
         stEvt:when="2016-05-13T15:05+02:00"
         stEvt:softwareAgent="Adobe Premiere Pro CC (Windows)"
         stEvt:changed="/"/>
       </rdf:Seq>
      </xmpMM:History>
      </rdf:Description>
     </rdf:li>

     //... (same <rdf:li> may appear several times!?)
     
    </rdf:Bag>
   </xmpMM:Pantry>

   //...
  
  </rdf:Description>
 </rdf:RDF>
</x:xmpmeta>


// cf. photosphere/pan360
<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">
  <rdf:Description rdf:about=\"\" xmlns:GPano=\"http://ns.google.com/photos/1.0/panorama/\">
    <GPano:UsePanoramaViewer>True</GPano:UsePanoramaViewer>
    <GPano:CaptureSoftware>HTC Pan360</GPano:CaptureSoftware>
    <GPano:StitchingSoftware>SE.Visual.Panostitcher</GPano:StitchingSoftware>
    <GPano:ProjectionType>equirectangular</GPano:ProjectionType>
    <GPano:PoseHeadingDegrees>0.0</GPano:PoseHeadingDegrees>
    <GPano:InitialHorizontalFOVDegrees>63.4</GPano:InitialHorizontalFOVDegrees>
    <GPano:CroppedAreaImageWidthPixels>4096</GPano:CroppedAreaImageWidthPixels>
    <GPano:CroppedAreaImageHeightPixels>2048</GPano:CroppedAreaImageHeightPixels>
    <GPano:FullPanoWidthPixels>4096</GPano:FullPanoWidthPixels>
    <GPano:FullPanoHeightPixels>2048</GPano:FullPanoHeightPixels>
    <GPano:CroppedAreaLeftPixels>0</GPano:CroppedAreaLeftPixels>
    <GPano:CroppedAreaTopPixels>0</GPano:CroppedAreaTopPixels>
  </rdf:Description>
</rdf:RDF>

*/

// uuid[ffcc8263-f855-4a93-8814-587a02521fdd]
static uint8 const SPHERICAL_UUID_ID[16] = { 0xff, 0xcc, 0x82, 0x63, 0xf8, 0x55, 0x4a, 0x93, 0x88, 0x14, 0x58, 0x7a, 0x02, 0x52, 0x1f, 0xdd };

// xmp uuid
static uint8 const XMP_UUID_ID[16] = { 0xbe, 0x7a, 0xcf, 0xcb, 0x97, 0xa9, 0x42, 0xe8, 0x9c, 0x71, 0x99, 0x94, 0x91, 0xe3, 0xaf, 0xac };

// CRCs
static uint32 const crcSpherical                    = mlabs::balai::CalcCRC("Spherical");
static uint32 const crcStitched                     = mlabs::balai::CalcCRC("Stitched");
static uint32 const crcStitchingSoftware            = mlabs::balai::CalcCRC("StitchingSoftware");
static uint32 const crcProjectionType               = mlabs::balai::CalcCRC("ProjectionType");
static uint32 const crcStereoMode                   = mlabs::balai::CalcCRC("StereoMode");
static uint32 const crcFullPanoWidthPixels          = mlabs::balai::CalcCRC("FullPanoWidthPixels");
static uint32 const crcFullPanoHeightPixels         = mlabs::balai::CalcCRC("FullPanoHeightPixels");
static uint32 const crcCroppedAreaImageWidthPixels  = mlabs::balai::CalcCRC("CroppedAreaImageWidthPixels");
static uint32 const crcCroppedAreaImageHeightPixels = mlabs::balai::CalcCRC("CroppedAreaImageHeightPixels");
static uint32 const crcCroppedAreaLeftPixels        = mlabs::balai::CalcCRC("CroppedAreaLeftPixels");
static uint32 const crcCroppedAreaTopPixels         = mlabs::balai::CalcCRC("CroppedAreaTopPixels");

struct GSpherical {
    // [optional]
    int FullPanoWidthPixels;
    int FullPanoHeightPixels;
    int CroppedAreaImageWidthPixels;
    int CroppedAreaImageHeightPixels;
    int CroppedAreaLeftPixels;
    int CroppedAreaTopPixels;

    // [required]
    uint8 Spherical;         // GSpherical:Spherical
    uint8 Stitched;          // GSpherical:Stitched = true
    uint8 StitchingSoftware; // GSpherical:StitchingSoftware = ANY value
    uint8 ProjectionType;    // GSpherical:ProjectionType = equirectangular

    GSpherical():
        FullPanoWidthPixels(0),FullPanoHeightPixels(0),
        CroppedAreaImageWidthPixels(0),CroppedAreaImageHeightPixels(0),
        CroppedAreaLeftPixels(0),CroppedAreaTopPixels(0),
        Spherical(0),Stitched(0),StitchingSoftware(0),ProjectionType(0) {}
    void Reset() {
        FullPanoWidthPixels = FullPanoHeightPixels =
        CroppedAreaImageWidthPixels = CroppedAreaImageHeightPixels =
        CroppedAreaLeftPixels = CroppedAreaTopPixels = 0;
        Spherical = Stitched = StitchingSoftware = ProjectionType = 0;
    }
    bool IsComplete() const {
        return (Spherical && Stitched && StitchingSoftware && ProjectionType);
    }
    bool Make(float hfov, float vfov_inf, float vfov_sup, int width, int height) {
        FullPanoWidthPixels          = (int)(width*360.0f/hfov);
        FullPanoHeightPixels         = (int)(height*180.0f/(vfov_sup-vfov_inf));
        CroppedAreaImageWidthPixels  = width;
        CroppedAreaImageHeightPixels = height;
        CroppedAreaLeftPixels        = (FullPanoWidthPixels-CroppedAreaImageWidthPixels+1)/2;
        CroppedAreaTopPixels         = (int)((FullPanoHeightPixels*(90-vfov_sup)+90)/180);
        return (15.0f<hfov) && (hfov<=360.0f) &&
               (-90.0f<=vfov_inf) && (vfov_inf<vfov_sup) && (vfov_sup<=90.0f) &&
               (0<width) && (width<65536) && (0<height) && (height<65536);
    }
    bool GetViewCoverage(int& lon, int& lat_inf, int& lat_sup) const {
        if (Spherical && Stitched && StitchingSoftware && ProjectionType) {
            if (0<FullPanoWidthPixels && 0<FullPanoHeightPixels &&
                (CroppedAreaLeftPixels+CroppedAreaImageWidthPixels)<=FullPanoWidthPixels) {
                lon = 360*CroppedAreaImageWidthPixels/FullPanoWidthPixels;
                lat_inf = 90 - 180*(CroppedAreaTopPixels+CroppedAreaImageHeightPixels)/FullPanoHeightPixels;
                lat_sup = 90 - 180*CroppedAreaTopPixels/FullPanoHeightPixels;
                if (lon<2 || lat_sup<=lat_inf)
                    return false;

                if (lon>358) lon = 360;
                if (lat_sup>88) lat_sup = 90;
                if (lat_inf<-88) lat_inf = -90;
            }
            else {
                lon = 360;
                lat_inf = -90;
                lat_sup = 90;
            }
            return true;
        }
        return false;
    }
};

struct GoogleJumpMetadata : public XMLParser {
    GSpherical spherical;
    uint8 spherical_xmlns;
    uint8 stereo_mode;

    GoogleJumpMetadata():spherical(),spherical_xmlns(0),stereo_mode(0xff) {}

    bool BeginTag_(XML_Element const& ele, XML_Element const* ascent) {
        if (NULL==ascent && 0==memcmp("rdf:SphericalVideo", ele.Tag, 19)) {
            if (ele.NumAttribs>=2) {
                bool xmlns_rdf = false;
                bool xmlns_GSpherical = false;
                for (int i=0; i<ele.NumAttribs; ++i) {
                    XML_Attrib const& attr = ele.Attributes[i];
                    if (!xmlns_rdf && 0==memcmp("xmlns:rdf", attr.Name, 10)) {
                        xmlns_rdf = (0==memcmp("http://www.w3.org/1999/02/22-rdf-syntax-ns#", attr.Value, 44));
                    }
                    if (!xmlns_GSpherical && 0==memcmp("xmlns:GSpherical", attr.Name, 17)) {
                        xmlns_GSpherical = (0==memcmp("http://ns.google.com/videos/1.0/spherical/", attr.Value, 43));
                    }
                }
                spherical_xmlns = xmlns_rdf && xmlns_GSpherical;
            }
        }

        return (0!=spherical_xmlns);
    }
    bool EndTag_(XML_Element const& ele, XML_Element const* ascent) {
        if (NULL!=ascent && 0==memcmp("GSpherical:", ele.Tag, 11)) {
            uint32 const crcTag = mlabs::balai::CalcCRC(ele.Tag + 11);
            if (crcSpherical==crcTag) {
                spherical.Spherical = (0==memcmp("true", ele.Content, 5));
            }
            else if (crcStitched==crcTag) {
                spherical.Stitched = (0==memcmp("true", ele.Content, 5));
            }
            else if (crcStitchingSoftware==crcTag) {
                spherical.StitchingSoftware = (NULL!=ele.Content);
            }
            else if (crcProjectionType==crcTag) {
                spherical.ProjectionType = (0==memcmp("equirectangular", ele.Content, 16));
                if (0==spherical.ProjectionType)
                    return false;
            }
            else if (crcStereoMode==crcTag) {
                if (0==memcmp("top-bottom", ele.Content, 11)) {
                    stereo_mode = 1;
                }
                else if (0==memcmp("left-right", ele.Content, 11)) {
                    stereo_mode = 2;
                }
                else {
                    assert(0==memcmp("mono", ele.Content, 5));
                    stereo_mode = 0;
                }
            }
            else if (crcCroppedAreaImageWidthPixels==crcTag) {
                spherical.CroppedAreaImageWidthPixels = atoi(ele.Content);
            }
            else if (crcCroppedAreaImageHeightPixels==crcTag) {
                spherical.CroppedAreaImageHeightPixels = atoi(ele.Content);
            }
            else if (crcFullPanoWidthPixels==crcTag) {
                spherical.FullPanoWidthPixels = atoi(ele.Content);
            }
            else if (crcFullPanoHeightPixels==crcTag) {
                spherical.FullPanoHeightPixels = atoi(ele.Content);
            }
            else if (crcCroppedAreaLeftPixels==crcTag) {
                spherical.CroppedAreaLeftPixels = atoi(ele.Content);
            }
            else if (crcCroppedAreaTopPixels==crcTag) {
                spherical.CroppedAreaTopPixels = atoi(ele.Content);
            }
            // more...
        }

        if (NULL==ascent && 0xff==stereo_mode && spherical.IsComplete()) {
            stereo_mode = 0;
        }

        return (0!=spherical_xmlns);
    }
};

struct VirtueRealPornMetadata : public XMLParser {
    GSpherical gpano;
    uint8 gpano_xmlns;
    uint8 gpano_read;
    uint8 stereo_mode;

    VirtueRealPornMetadata():gpano(),gpano_xmlns(0),gpano_read(0),stereo_mode(0) {}

    bool BeginTag_(XML_Element const& ele, XML_Element const*) {
        if (0==gpano_read && 0==memcmp("rdf:Description", ele.Tag, 16)) {
            if (gpano_xmlns) {
                gpano.Reset();
                for (int i=0; i<ele.NumAttribs; ++i) {
                    XML_Attrib const& attr = ele.Attributes[i];
                    if (0==memcmp("GPano:", attr.Name, 6)) {
                        uint32 const crcTag = mlabs::balai::CalcCRC(attr.Name + 6);
                        if (crcProjectionType==crcTag) {
                            gpano.ProjectionType = 0==memcmp("equirectangular", attr.Value, 16);
                            if (gpano.ProjectionType) {
                                gpano.Stitched = true;
                                gpano.Spherical = true;
                                gpano.StitchingSoftware = true; // not required
                            }
                        }
                        else if (crcCroppedAreaImageWidthPixels==crcTag) {
                            gpano.CroppedAreaImageWidthPixels = atoi(attr.Value);
                        }
                        else if (crcCroppedAreaImageHeightPixels==crcTag) {
                            gpano.CroppedAreaImageHeightPixels = atoi(attr.Value);
                        }
                        else if (crcFullPanoWidthPixels==crcTag) {
                            gpano.FullPanoWidthPixels = atoi(attr.Value);
                        }
                        else if (crcFullPanoHeightPixels==crcTag) {
                            gpano.FullPanoHeightPixels = atoi(attr.Value);
                        }
                        else if (crcCroppedAreaLeftPixels==crcTag) {
                            gpano.CroppedAreaLeftPixels = atoi(attr.Value);
                        }
                        else if (crcCroppedAreaTopPixels==crcTag) {
                            gpano.CroppedAreaTopPixels = atoi(attr.Value);
                        }
                    }
                }

                int lon(0), lat_inf(0), lat_sup(0);
                if (gpano.GetViewCoverage(lon, lat_inf, lat_sup)) {
                    gpano_read = true;
                    stereo_mode = 2; // 2 left-right
                }
                else {
                    gpano.Reset();
                }
            }
            else {
                for (int i=0; i<ele.NumAttribs; ++i) {
                    XML_Attrib const& attr = ele.Attributes[i];
                    if (0==memcmp("xmlns:GPano", attr.Name, 12) &&
                        0==memcmp("http://ns.google.com/photos/1.0/panorama/", attr.Value, 42)) {
                        gpano_xmlns = 1;
                        break;
                    }
                }
            }
        }

        return true;
    }
    bool EndTag_(XML_Element const&, XML_Element const*) { return true; }
};

struct FB360Metadata : public XMLParser {

    int track_info;
    FB360Metadata():track_info(-1) {}
/*

<fb360>
 <audio>
  <AudioChannelConfiguration schemeIdUri="tag:facebook.com,2016-08-16:fb360:audio:channel_layout" value="tbe_8a" />
 </audio>
</fb360>

<fb360>
 <audio>
  <AudioChannelConfiguration schemeIdUri="tag:facebook.com,2016-08-16:fb360:audio:channel_layout" value="tbe_8b" />
 </audio>
</fb360>

<fb360>
 <audio>
  <!-- "nda" is temporary, while we transition server code -->
  <AudioChannelConfiguration schemeIdUri="tag:facebook.com,2016-08-16:fb360:audio:channel_layout" value="nda" />
  <AudioChannelConfiguration schemeIdUri="tag:facebook.com,2016-08-16:fb360:audio:channel_layout" value="headlocked" />
 </audio>
</fb360>

*/
    bool BeginTag_(XML_Element const& ele, XML_Element const* ascent) {
        if (NULL==ascent) {
            if (0!=memcmp("fb360", ele.Tag, 6)) {
                return false;
            }
        }

        return true;
    }
    bool EndTag_(XML_Element const& ele, XML_Element const* ascent) {
        if (NULL!=ascent) {
            if (-1==track_info &&
                0==memcmp("AudioChannelConfiguration", ele.Tag, 26)) {
                for (int i=0; i<ele.NumAttribs; ++i) {
                    XML_Attrib const& attr = ele.Attributes[i];
                    if (0==memcmp("value", attr.Name, 6)) {
                       if (0==memcmp("tbe_8a", attr.Value, 7)) {
                           track_info = 0;
                           break;
                       }
                       else if (0==memcmp("tbe_8b", attr.Value, 7)) {
                           track_info = 1;
                           break;
                       }
                       else if (0==memcmp("headlocked", attr.Value, 11)) {
                           track_info = 2;
                           break;
                       }
                    }
                }
            }
        }
        else {
            return 0==memcmp("fb360", ele.Tag, 6);
        }
        return true;
    }
};
//
// andre : have no chances to verify mkv spherical video yet.
//
struct EBML_GoogleJump {
    int  longitude;
    int  latitude_south;
    int  latitude_north;
    int  stereo_mode;

    bool check_track;
    bool check_spherical_uuid;
    bool check_spherical_video_tag;

    EBML_GoogleJump():longitude(0),latitude_south(0),latitude_north(0),stereo_mode(0),
        check_track(false),check_spherical_uuid(false),check_spherical_video_tag(false) {}

    bool IsValid() const {
        return check_track && check_spherical_uuid && check_spherical_video_tag &&
               0<longitude && -90<=latitude_south && latitude_south<latitude_north && latitude_north<=90;
    }

    bool ParseEBML_Target_(FILE* file, uint64 data_size) {
        // https://www.matroska.org/technical/specs/index.html#Targets
        uint64 read_bytes(0), size;
        uint32 a, b, header;
        while (read_bytes<data_size) {
            header = EBML_element_header(a, file); read_bytes += a;
            size = EBML_element_size(b, file); read_bytes += b;
            if (0x63ca==header && size>=5) { // TargetType, "Track"
                char track[6]; track[5] = '\0';
                if (5==fread(track, 1, 5, file)) {
                    if (0==memcmp(track, "Track", 5)) {
                        check_track = true;
                    }

                    if (size>5) {
                        _fseeki64(file, size-5, SEEK_CUR);
                    }
                }
                else {
                    return false;
                }
            }
            else if (0x63c5==header && size>=sizeof(SPHERICAL_UUID_ID)) { // TagTrackUID
                uint8 uuid[sizeof(SPHERICAL_UUID_ID)];
                if (sizeof(SPHERICAL_UUID_ID)==fread(uuid, 1, sizeof(SPHERICAL_UUID_ID), file)) {
                    if (0==memcmp(uuid, SPHERICAL_UUID_ID, sizeof(SPHERICAL_UUID_ID))) {
                        check_spherical_uuid = true;
                    }
                }
                else {
                    return false;
                }

                if (size>sizeof(SPHERICAL_UUID_ID)) {
                    _fseeki64(file, size - sizeof(SPHERICAL_UUID_ID), SEEK_CUR);
                }
            }
            else {
                _fseeki64(file, size, SEEK_CUR);
            }

            read_bytes += size;
        }

        return (read_bytes==data_size);
    }
    bool ParseEBML_SimpleTag_(FILE* file, uint64 data_size) {
        // https://www.matroska.org/technical/specs/index.html#SimpleTag
        longitude = latitude_south = latitude_north = 0;
        stereo_mode = -1;

        uint64 read_bytes(0), size;
        uint32 a, b, header;
        while (read_bytes<data_size) {
            header = EBML_element_header(a, file); read_bytes += a;
            size = EBML_element_size(b, file); read_bytes += b;
            if (0x45a3==header && size>=15) { // TagName "spherical-video" or "SPHERICAL-VIDEO"
                char tag[16]; tag[15] = '\0';
                if (15==fread(tag, 1, 15, file)) {
                    if (0==memcmp(tag, "spherical-video", 15) ||
                        0==memcmp(tag, "SPHERICAL-VIDEO", 15)) {
                        check_spherical_video_tag = true;
                    }

                    if (size>15) {
                        _fseeki64(file, size-15, SEEK_CUR);
                    }
                }
                else {
                    return false;
                }
            }
            else if (0x4487==header && size>0) { // TagString <xml data>, "utf8"
                void* const buf = malloc((size_t)size);
                if (NULL!=buf && size==fread(buf, 1, (size_t)size, file)) {
                    char const* xml_begin = (char const*) buf;
                    char const* xml_end = xml_begin + size;

                    // may have header if made by Google's "Spherical Metadata Tool".
                    char const* xml_header = "<?xml version=\"1.0\"?>"; // length = 21
                    if (0==memcmp(xml_begin, xml_header, 21))
                        xml_begin += 21;

                    while ('<'!=*xml_begin && xml_begin<xml_end)
                        ++xml_begin;

                    while ('>'!=xml_end[-1] && xml_begin<xml_end)
                        --xml_end;

                    GoogleJumpMetadata GJump;
                    if (GJump.Parse(xml_begin, xml_end) &&
                        GJump.spherical.GetViewCoverage(longitude,
                                                        latitude_south,
                                                        latitude_north)) {
                        stereo_mode = GJump.stereo_mode;
                    }
                }

                free(buf);
            }
            else {
                _fseeki64(file, size, SEEK_CUR);
            }

            read_bytes += size;
        }

        return (read_bytes==data_size);
    }

    bool Parse(FILE* file, uint64 data_size) {
        // https://www.matroska.org/technical/specs/index.html#Tag
        // https://github.com/google/spatial-media/blob/master/docs/spherical-video-rfc.md
        uint64 read_bytes(0), size;
        uint32 a, b, header;
        while (read_bytes<data_size) {
            header = EBML_element_header(a, file); read_bytes += a;
            size = EBML_element_size(b, file); read_bytes += b;
            if (0x63c0==header) {
                if (!ParseEBML_Target_(file, size))
                    return false;
            }
            else if (0x67c8==header) { // SimpleTag
                if (!ParseEBML_SimpleTag_(file, size))
                    return false;
            }
            else {
                _fseeki64(file, size, SEEK_CUR);
            }

            read_bytes += size;
        }

        return (read_bytes==data_size);
    }

    bool ParseV2(FILE* file, uint64 data_size) {
#if 0
        // [Projection]
        //   [ProjectionType value = 1]
        //   [ProjectionPrivate]
        //       flags = 0
        //       version = 0
        //       projection_bounds_top = 0
        //       projection_bounds_bottom = 0
        //       projection_bounds_left = 0
        //       projection_bounds_right = 0
        uint64 read_bytes(0), size;
        uint32 a, b, header;
        while (read_bytes<data_size) {
            header = EBML_element_header(a, file); read_bytes += a;
            size = EBML_element_size(b, file); read_bytes += b;
            if (0x7671==header) { // ProjectionType(Mandatory)
                // ProjectionType is an enum(uinteger). The valid values are:
                //  0: Rectangular
                //  1: Equirectangular
                //  2: Cubemap
                //  3: Mesh
                //
                 _fseeki64(file, size, SEEK_CUR);
            }
            else if (0x7672==header) { // ProjectionPrivate(optional)
                //
                // If ProjectionType equals 0 (Rectangular), then this element must NOT be present.
                //
                // If ProjectionPrivate presents, it contains binary data respect to ProjectionType:
                //   Equirectangular Projection Box ('equi') for Equirectangular
                //   Cubemap Projection Box ('cbmp') for Cubemap
                //   Mesh Projection Box ('mshp') for Mesh
                 _fseeki64(file, size, SEEK_CUR);
            }
            else if (0x7673==header) { // ProjectionPoseYaw(Mandatory)
                // float
                 _fseeki64(file, size, SEEK_CUR);
            }
            else if (0x7674==header) { // ProjectionPosePitch(Mandatory)
                // float
                 _fseeki64(file, size, SEEK_CUR);
            }
            else if (0x7675==header) { // ProjectionPoseRoll(Mandatory)
                // float
                 _fseeki64(file, size, SEEK_CUR);
            }
            else {
                _fseeki64(file, size, SEEK_CUR);
            }

            read_bytes += size;
        }

        return (read_bytes==data_size);
#else
        BL_LOG("[TO-DO : Spherical Video V2 RFC WebM (Matroska)] Projection master element(0x7670) is coming!\n");
        _fseeki64(file, data_size, SEEK_CUR);
        return true;
#endif
    }
};

class VideoMetadata
{
    uint64 file_size_;
    uint64 mdat_pos_, mdat_size_;
    int  nalu_length_size_;
    int parsing_track_no_; // 1-base

    htc::SA3D sa3d_;

    htc::VIDEO_SOURCE metadata_source_;
    int  longitude_;
    int  latitude_south_;
    int  latitude_north_;
    int  stereo_mode_; // stereo3D 0:mono 1:top-bottom 2:left-right 3:bottom-top 4:right-left

    uint8 ftyp_; // type check : 1="ftyp", 2=Matroska, 3:RIFF
    uint8 moov_; // "moov"
    uint8 full3D_; // 0:not specified, 1:half, 2:full
    uint8 sand_; // non-diegetic audio box - SAND

    bool moov_trak_mdia_minf_stbl_stsd_VideoSampleEntry(FILE* file, uint8 const* name, uint32 size) {
/*
        // ISO/IEC 14496-15. clause 5.2.4.1.1, H.264
        aligned(8) class AVCDecoderConfigurationRecord {
            unsigned int(8) configurationVersion = 1;
            unsigned int(8) AVCProfileIndication;
            unsigned int(8) profile_compatibility; // likely 0
            unsigned int(8) AVCLevelIndication;    // 0x1f(31)
            bit(6) reserved = ．111111・b;
            unsigned int(2) lengthSizeMinusOne;  // length in bytes of the NALUnitLength field
            bit(3) reserved = ．111・b;
            unsigned int(5) numOfSequenceParameterSets;
            for (i=0; i< numOfSequenceParameterSets; i++) {
                unsigned int(16) sequenceParameterSetLength ;
                bit(8*sequenceParameterSetLength) sequenceParameterSetNALUnit;
            }
            unsigned int(8) numOfPictureParameterSets;
            for (i=0; i< numOfPictureParameterSets; i++) {
                unsigned int(16) pictureParameterSetLength;
                bit(8*pictureParameterSetLength) pictureParameterSetNALUnit;
            }
            if (profile_idc == 100 || profile_idc == 110 ||
                profile_idc == 122 || profile_idc == 144 )
            {
                bit(6) reserved = ．111111・b;
                unsigned int(2) chroma_format;
                bit(5) reserved = ．11111・b;
                unsigned int(3) bit_depth_luma_minus8;
                bit(5) reserved = ．11111・b;
                unsigned int(3) bit_depth_chroma_minus8;
                unsigned int(8) numOfSequenceParameterSetExt;
                for (i=0; i< numOfSequenceParameterSetExt; i++) {
                    unsigned int(16) sequenceParameterSetExtLength;
                    bit(8*sequenceParameterSetExtLength) sequenceParameterSetExtNALUnit;
                }
            }
        }

        // ISO/IEC 14496-15:2013(E) 8.3.3.1.2 Syntax, H.265
        aligned(8) class HEVCDecoderConfigurationRecord {
            unsigned int(8) configurationVersion = 1;
            unsigned int(2) general_profile_space;
            unsigned int(1) general_tier_flag;
            unsigned int(5) general_profile_idc;
            unsigned int(32) general_profile_compatibility_flags;
            unsigned int(48) general_constraint_indicator_flags;
            unsigned int(8) general_level_idc;
            bit(4) reserved = ．1111・b;
            unsigned int(12) min_spatial_segmentation_idc;
            bit(6) reserved = ．111111・b;
            unsigned int(2) parallelismType;
            bit(6) reserved = ．111111・b;
            unsigned int(2) chromaFormat;
            bit(5) reserved = ．11111・b;
            unsigned int(3) bitDepthLumaMinus8;
            bit(5) reserved = ．11111・b;
            unsigned int(3) bitDepthChromaMinus8;
            bit(16) avgFrameRate;
            bit(2) constantFrameRate;
            bit(3) numTemporalLayers;
            bit(1) temporalIdNested;
            unsigned int(2) lengthSizeMinusOne;
            unsigned int(8) numOfArrays;
            for (j=0; j < numOfArrays; j++) {
                bit(1) array_completeness;
                unsigned int(1) reserved = 0;
                unsigned int(6) NAL_unit_type;
                unsigned int(16) numNalus;
                for (i=0; i< numNalus; i++) {
                    unsigned int(16) nalUnitLength;
                    bit(8*nalUnitLength) nalUnit;
                }
            }
        }

        // ISO/IEC 14496-15. clause 5.3.4.1 Sample description name and format
        class AVCConfigurationBox extends Box(．avcC・) {
            AVCDecoderConfigurationRecord() AVCConfig;
        }

        class AVCSampleEntry() extends VisualSampleEntry('avc1') {
            AVCConfigurationBox config;
            MPEG4BitRateBox();              // optional
            MPEG4ExtensionDescriptorsBox(); // optional
        }

        class AVC2SampleEntry() extends VisualSampleEntry('avc2') {
            AVCConfigurationBox avcconfig;
            MPEG4BitRateBox bitrate;            // optional
            MPEG4ExtensionDescriptorsBox descr; // optional
            extra_boxes boxes;                  // optional
        }

        // ISO/IEC 14496-12. clause 8.16 Sample Description Box
        aligned(8) abstract class SampleEntry(unsigned int(32) format) extends Box(format) {
            const unsigned int(8)[6] reserved = 0;
            unsigned int(16) data_reference_index;
        }

        // Visual Sequences
        class VisualSampleEntry(codingname) extends SampleEntry(codingname) {
            //
            // 8 bytes for Box (processed)
            //
            // [0:7] SampleEntry
            //
            unsigned int(16) pre_defined = 0;       // [8:9]
            const unsigned int(16) reserved = 0;    // [10:11]
            unsigned int(32)[3] pre_defined = 0;    // [12:23]
            unsigned int(16) width;                 // [24:25]
            unsigned int(16) height;                // [16:27]
            template unsigned int(32) horizresolution = 0x00480000; // [28:31] // 72 dpi 
            template unsigned int(32) vertresolution = 0x00480000;  // [32:35]  // 72 dpi
            const unsigned int(32) reserved = 0;    // [36:39]
            template unsigned int(16) frame_count = 1; // [40:41]
            string[32] compressorname;                 // [42:73]
            template unsigned int(16) depth = 0x0018;  // [74:75]
            int(16) pre_defined = -1;                  // [76:77]
        }

        // Audio Sequences
        class AudioSampleEntry(codingname) extends SampleEntry(codingname) {
            const unsigned int(32)[2] reserved = 0;
            template unsigned int(16) channelcount = 2;
            template unsigned int(16) samplesize = 16;
            unsigned int(16) pre_defined = 0;
            const unsigned int(16) reserved = 0 ;
            template unsigned int(32) samplerate = {timescale of media}<<16;
        }

        // ISO/IEC 14496-12. 4.2 Object Structure
        // An object in this terminology is a box.
        aligned(8) class Box (unsigned int(32) boxtype,
                              optional unsigned int(8)[16] extended_type) {
            unsigned int(32) size;
            unsigned int(32) type = boxtype;
            if (size==1) {
                unsigned int(64) largesize;
            } else if (size==0) {
                // box extends to end of file
            }
            if (boxtype=='uuid') {
                unsigned int(8)[16] usertype = extended_type;
            }
        }

        aligned(8) class FullBox(unsigned int(32) boxtype, unsigned int(8) v, bit(24) f)
            extends Box(boxtype) {
            unsigned int(8) version = v;
            bit(24) flags = f;
        }

        // https://github.com/google/spatial-media/blob/master/docs/spherical-video-v2-rfc.md
        As the V2 specification stores its metadata in a different location, it is possible
        for a file to contain both the V1 and V2 metadata. If both V1 and V2 metadata are contained
        they should contain semantically equivalent information, with V2 taking priority when they differ.

        //
        // Stereoscopic 3D Video Box (st3d) stores additional information about stereoscopic
        // rendering in this video track. This box must come after non-optional boxes defined
        // by the ISOBMFF specification and before optional boxes at the end of the
        // VisualSampleEntry definition such as the CleanApertureBox and PixelAspectRatioBox(pasp).
        aligned(8) class Stereoscopic3D extends FullBox('st3d', 0, 0) {
            unsigned int(8) stereo_mode; // 0:mono, 1:top-bottom, 2:left-right
        }

        //
        // Spherical Video Box (sv3d) stores additional information about spherical video content
        // contained in this video track. This box must come after non-optional boxes defined
        // by the ISOBMFF specification and before optional boxes at the end of the VisualSampleEntry
        // definition such as the CleanApertureBox and PixelAspectRatioBox.
        // This box should be placed after the Stereoscopic3D box if one is present.
        aligned(8) class SphericalVideoBox extends Box('sv3d') {
            SphericalVideoHeader svhd; // mandatory
            Projection           proj; // mandatory

        }

        // Spherical Video Header (svhd)
        aligned(8) class SphericalVideoHeader extends FullBox('svhd', 0, 0) {
            string metadata_source;
        }

        // Projection Box (proj)
        aligned(8) class Projection extends Box('proj') {
            ProjectionHeader prhd; // mandatory
            CubemapProjection or EquirectangularProjection or MeshProjection proj_data; // mandatory 
        }

        // Projection Header Box (prhd)
        aligned(8) class ProjectionHeader extends FullBox('prhd', 0, 0) {
            int(32) pose_yaw_degrees;
            int(32) pose_pitch_degrees;
            int(32) pose_roll_degrees;
        }

        // Projection Data Box - 3 kinds of Projection Data Boxes followed
        aligned(8) class ProjectionDataBox(unsigned int(32) proj_type, unsigned int(32) version, unsigned int(32) flags)
            extends FullBox(proj_type, version, flags) {
        }

        // Cubemap Projection Box (cbmp)
        aligned(8) class CubemapProjection extends ProjectionDataBox('cbmp', 0, 0) {
            unsigned int(32) layout;
            unsigned int(32) padding;
        }

        // Equirectangular Projection Box (equi)
        aligned(8) class EquirectangularProjection extends ProjectionDataBox('equi', 0, 0) {
            unsigned int(32) projection_bounds_top;
            unsigned int(32) projection_bounds_bottom;
            unsigned int(32) projection_bounds_left;
            unsigned int(32) projection_bounds_right;
        }

        // Mesh Projection Box (mshp)
        aligned(8) class MeshProjection extends ProjectionDataBox('mshp', 0, 0) {
            unsigned int(32) crc;
            unsigned int(32) encoding_four_cc;

            // All bytes below this point are compressed according to
            // the algorithm specified by the encoding_four_cc field.
            MeshBox() meshes[]; // At least 1 mesh box must be present.
            Box(); // further boxes as needed
        }

        // Mesh Box (mesh)
        aligned(8) class Mesh extends Box(．mesh・) {
            const unsigned int(1) reserved = 0;
            unsigned int(31) coordinate_count;
            for (i = 0; i < coordinate_count; i++) {
              float(32) coordinate;
            }
            const unsigned int(1) reserved = 0;
            unsigned int(31) vertex_count;
            for (i = 0; i < vertex_count; i++) {
              unsigned int(ccsb) x_index_delta;
              unsigned int(ccsb) y_index_delta;
              unsigned int(ccsb) z_index_delta;
              unsigned int(ccsb) u_index_delta;
              unsigned int(ccsb) v_index_delta;
            }
            const unsigned int(1) padding[];

            const unsigned int(1) reserved = 0;
            unsigned int(31) vertex_list_count;
            for (i = 0; i < vertex_list_count; i++) {
              unsigned int(8) texture_id;
              unsigned int(8) index_type;
              const unsigned int(1) reserved = 0;
              unsigned int(31) index_count;
              for (j = 0; j < index_count; j++) {
                unsigned int(vcsb) index_as_delta;
              }
              const unsigned int(1) padding[];
            }
        }

        //
        // ref ISO/IEC 14496-12. 6.2.3 Box Order
        //
        // e.g. layout
        //
        // [moov: Movie Box]
        //  [trak: track or stream] // a track box should be here! 2016.12.23 -andre
        //   [mdia: Media Box]
        //     [minf: Media Information Box]
        //       [stbl: Sample Table Box]
        //         [stsd: Sample Table Sample Descriptor]
        //           [avc1: Advance Video Coding Box]
        //             [VisualSampleEntry (78 bytes) here]
        //             [avcC: AVC Configuration Box]
        //               ...
        //             [st3d: Stereoscopic 3D Video Box]
        //             [sv3d: Spherical Video Box]
        //               [svhd: Spherical Video Header Box]
        //               [proj: Projection Box]
        //                 [prhd: Projection Header Box]
        //                 [equi: Equirectangular Projection Box]
        //             [pasp: Pixel Aspect Ratio Box]
        //
*/
        if (size>92) { // 78 + 8 + min sizeof(AVCDecoderConfigurationRecord)
            uint8 buf[128];

            // VisualSampleEntry 
            uint32 read_bytes = (uint32) fread(buf, 1, 78, file);
            int const width = buf[24]<<8 | buf[25];
            int const height = buf[26]<<8 | buf[27];
            if (78!=read_bytes || width<=3 || height<2) {
                _fseeki64(file, size-read_bytes, SEEK_CUR);
                return true;
            }

#ifdef BL_DEBUG_BUILD
            uint32 const horizresolution = buf[28]<<24 | buf[29]<<16 | buf[30]<<8 | buf[31];
            uint32 const vertresolution = buf[32]<<24 | buf[33]<<16 | buf[34]<<8 | buf[35];
            uint16 const frame_count = buf[40]<<8 | buf[41]; // frame_count(1)
            uint16 const depth = buf[74]<<8 | buf[75]; // 0x0018(24)
            int16 pre_defined = buf[76]<<8 | buf[77];  // 0xffff(-1)
            if (0x00480000!=horizresolution) {
                BL_ERR("VisualSampleEntry(%s) check - horizresolution=0x%X(0x00480000=72dpi)\n", name, horizresolution);
            }
            if (0x00480000!=vertresolution) {
                BL_ERR("VisualSampleEntry(%s) check - vertresolution=0x%X(0x00480000=72dpi)\n", name, vertresolution);
            }
            if (1!=frame_count) {
                BL_ERR("VisualSampleEntry(%s) check - frame_count=%d(1)\n", name, frame_count);
            }
            if (0x18!=depth) {
                BL_ERR("VisualSampleEntry(%s) check - depth=0x%X(0x0018)\n", name, depth);
            }
            if (-1!=pre_defined) {
                BL_ERR("VisualSampleEntry(%s) check - pre_defined=%d(-1)\n", name, pre_defined);
            }
#else
            (name); // reference parameter
#endif
            uint32 rsize;
            uint8 hdr[9]; hdr[8] = '\0';
            while ((read_bytes+8)<=size && 8==fread(hdr, 1, 8, file)) {
                uint32 const boxSize = BL_MAKE_4CC(hdr[0], hdr[1], hdr[2], hdr[3]);
                uint32 const dataSize = boxSize - 8;
                if (0==memcmp(hdr+4, "avcC", 4)) {
                    if (dataSize<=sizeof(buf)) {
                        rsize = (uint32) fread(buf, 1, dataSize, file);
                    }
                    else {
                        rsize = (uint32) fread(buf, 1, 8, file);
                    }

                    if (rsize<dataSize) {
                        _fseeki64(file, dataSize-rsize, SEEK_CUR);
                    }

                    // AVCDecoderConfigurationRecord loaded
                    if (rsize>6 &&
                        1==buf[0] /*configurationVersion = 1*/ &&
                        0xfc==(0xfc&buf[4]) && 0xe0==(0xe0&buf[5])) {

                        // nalu length size
                        nalu_length_size_ = (0x03&buf[4]) + 1;
#if 0
                        int const profile_idc = buf[1]; // AVCProfileIndication

                        // sps
                        int const numOfSequenceParameterSets = 0x1f&buf[5];
                        ptr = buf + 6;
                        for (int i=0; i<numOfSequenceParameterSets; ++i) {
                            uint32 const sequenceParameterSetLength = ptr[0]<<8 | ptr[1];
                            ptr += 2;
                            ptr += sequenceParameterSetLength;
                        }

                        int const numOfPictureParameterSets = *ptr++;
                        for (int i=0; i< numOfPictureParameterSets; ++i) {
                            uint32 const pictureParameterSetLength = ptr[0]<<8 | ptr[1];
                            ptr += 2;
                            ptr += pictureParameterSetLength;
                        }

                        if (profile_idc == 100 || profile_idc == 110 ||
                            profile_idc == 122 || profile_idc == 144 ) {
                            if (0xfc==(0xfc&ptr[0])) {
                                int xxx = 0x03&ptr[0]; /*chroma_format*/ ++ptr;
                                if (0xf8==(0xf8&ptr[0])) {
                                    xxx = 0x07&ptr[0]; /*bit_depth_luma_minus8*/ ++ptr;
                                    if (0xf8==(0xf8&ptr[0])) {
                                        xxx = 0x07&ptr[0]; /*bit_depth_chroma_minus8*/ ++ptr;
                                        int const numOfSequenceParameterSetExt = *ptr++;
                                        for (int i=0; i<numOfSequenceParameterSetExt; ++i) {
                                            int sequenceParameterSetExtLength = ptr[0]<<8 | ptr[1];
                                            ptr += 2;
                                            ptr += sequenceParameterSetExtLength;
                                        }
                                    }
                                }
                            }
                        }
#endif
                    }
                }
                else if (0==memcmp(hdr+4, "hvcC", 4)) {
                    if (dataSize<=sizeof(buf)) {
                        rsize = (uint32) fread(buf, 1, dataSize, file);
                    }
                    else {
                        rsize = (uint32) fread(buf, 1, 8, file);
                    }

                    if (rsize<dataSize) {
                        _fseeki64(file, dataSize-rsize, SEEK_CUR);
                    }

                    if (23<=rsize &&
                        1==buf[0] /*configurationVersion = 1*/ &&
                        0xf0==(0xf0&buf[13]) && 0xfc==(0xfc&buf[15]) && 0xfc==(0xfc&buf[16]) &&
                        0xf8==(0xf8&buf[17]) && 0xf8==(0xf8&buf[18])) {
                        nalu_length_size_ = (0x03&buf[21]) + 1;
                    }
                }
                else if (0==memcmp(hdr+4, "st3d", 4)) { // Stereoscopic 3D Video Box (st3d)
                    // This box must come after non-optional boxes defined
                    // by the ISOBMFF specification and before optional boxes at the end of the
                    // VisualSampleEntry definition such as the CleanApertureBox and PixelAspectRatioBox(pasp).
                    //
                    // FullBox:
                    //  unsigned int(8) version;
                    //  bit(24) flags;
                    //
                    //  unsigned int(8) stereo_mode; // 0:mono, 1:top-bottom, 2:left-right
                    //
                    // 0 : Monoscopic, a single monoscopic view.
                    // 1 : Top-Bottom, a stereoscopic view storing the left eye on top half of the frame and right eye at the bottom half of the frame.
                    // 2 : Left-Right, a stereoscopic view storing the left eye on left half of the frame and right eye on the right half of the frame.
                    // 3 : Stereo-Mesh, a stereoscopic view where the frame layout for left and right eyes are encoded in the (u,v) coordinates of two meshes. This may only be used with a mesh projection that contains a mesh for each eye.
                    //
                    if (dataSize<=sizeof(buf)) {
                        rsize = (uint32) fread(buf, 1, dataSize, file);
                    }
                    else {
                        rsize = (uint32) fread(buf, 1, sizeof(buf), file);
                    }
                    if (rsize<dataSize) {
                        _fseeki64(file, dataSize-rsize, SEEK_CUR);
                    }

                    if (0==buf[0]) { // mono
                        stereo_mode_ = 0;
                    }
                    else if (1==buf[0]) { // top-down
                        stereo_mode_ = 1;
                    }
                    else if (2==buf[0]) { // left-right
                        stereo_mode_ = 2;
                    }
                    else if (3==buf[0]) {
                        BL_LOG("Stereoscopic 3D Video Box (st3d) \"Stereo-Mesh\" not support!\n");
                    }
                    else {
                        BL_LOG("Stereoscopic 3D Video Box (st3d) with illegal value(%d)!\n", buf[0]);
                    }
                }
                else if (0==memcmp(hdr+4, "sv3d", 4)) { // Spherical Video Box (sv3d)
                    // This box should be placed 'after' the Stereoscopic3D box if one is present.
                    // and before optional boxes at the end of the VisualSampleEntry
                    // definition such as the CleanApertureBox and PixelAspectRatioBox(pasp)
                    //
                    // (let VIDEO_SOURCE = VIDEO_SOURCE_GOOGLE_JUMP_RFC2 if loaded successfully)
                    //
                    BL_LOG("[TO-DO : Spherical Video V2 RFC] Spherical Video Box (sv3d) is coming!\n");
                    _fseeki64(file, dataSize, SEEK_CUR);
                }
                else {
                    _fseeki64(file, dataSize, SEEK_CUR);
                    //if (0==memcmp(hdr+4, "pasp", 4)) {
                        //read_bytes += boxSize;
                        //break; // may break follow assert!
                    //}
                }
                read_bytes += boxSize;
            }

            assert(read_bytes==size || (read_bytes+4)==size);
            if (read_bytes<=size) {
                if (read_bytes<size) {
                    _fseeki64(file, size-read_bytes, SEEK_CUR);
                }
                return true;
            }

            return false;
        }
        else {
            _fseeki64(file, size, SEEK_CUR);
        }
        return true;
    }

    bool moov_trak_mdia_minf_stbl_stsd_AudioSampleEntry(FILE* file, uint8 const* name, uint32 size) {
#if 0
        //
        // this is taken from Google's spatial-media Spatial Audio RFC2.
        // https://github.com/google/spatial-media/blob/master/spatialmedia/mpeg/constants.py
        //
        uint32 const sound_sample_names[] = {
            MAKE_BOX4CC('m', 'p', '4', 'a'),
            MAKE_BOX4CC('l', 'p', 'c', 'm'),
            MAKE_BOX4CC('r', 'a', 'w', ' '),
            MAKE_BOX4CC('t', 'w', 'o', 's'),
            MAKE_BOX4CC('s', 'o', 'w', 't'),
            MAKE_BOX4CC('f', 'l', '3', '2'),
            MAKE_BOX4CC('f', 'l', '6', '4'),
            MAKE_BOX4CC('i', 'n', '2', '4'),
            MAKE_BOX4CC('i', 'n', '3', '2'),
            MAKE_BOX4CC('u', 'l', 'a', 'w'),
            MAKE_BOX4CC('a', 'l', 'a', 'w'),
            MAKE_BOX4CC('N', 'O', 'N', 'E'),
        };
        int const sound_sample_types = sizeof(sound_sample_names)/sizeof(sound_sample_names[0]);
        uint32 const sound_sample_name = MAKE_BOX4CC(name[0], name[1], name[2], name[3]);
        int type = sound_sample_types;
        for (int i=0; i<sound_sample_types; ++i) {
            if (sound_sample_name==sound_sample_names[i]) {
                type = i;
                break;
            }
        }

        if (type>=sound_sample_types || size<36) {
            // name='twos' or 'rtmd'
            if (size>0) {
                _fseeki64(file, size, SEEK_CUR);
            }
            return true;
        }
#endif

        // we hit a kind of AudioSampleEntry box, so the minimum size is 28
/*
        class AudioSampleEntry(codingname) extends SampleEntry(codingname) {
            //
            // 8 bytes for Box (processed)
            //
            // [0:7] SampleEntry
            //   const unsigned int(8)[6] reserved = 0;
            //   unsigned int(16) data_reference_index;
            //
            const unsigned int(32)[2] reserved = 0;     // [8:15]
            template unsigned int(16) channelcount = 2; // [16:17]
            template unsigned int(16) samplesize = 16;  // [18:19]
            unsigned int(16) pre_defined = 0;           // [20:21]
            const unsigned int(16) reserved = 0 ;       // [22:23]
            template unsigned int(32) samplerate = {timescale of media}<<16; // [24:27]
        }
*/
        // capacity for 3rd(16 channels) Ambisonics
        int const min_SA3D_size = 12 + 4*4;  // first order ambisonic
        if (size<(28+8+min_SA3D_size)) {
            if (size>0) {
                _fseeki64(file, size, SEEK_CUR);
            }
            return true;
        }

        int const max_SA3D_size = 12 + 4*16; // 3rd order ambisonic
        uint8 buf[9+max_SA3D_size];

        // read AudioSampleEntry
        uint32 read_bytes = (uint32) fread(buf, 1, 28, file);
        if (28!=read_bytes) {
            _fseeki64(file, size-read_bytes, SEEK_CUR);
            return true;
        }

        int const sample_desc_version = buf[8]<<8 | buf[9];
        int channel_count = buf[16]<<8 | buf[17]; // can be 2, even if ambisonic(4)
#ifdef BL_DEBUG_BUILD
        uint32 const reserved1 = buf[8]<<24 | buf[9]<<16 | buf[10]<<8 | buf[11];
        uint32 const reserved2 = buf[12]<<24 | buf[13]<<16 | buf[14]<<8 | buf[15];
        //uint32 const samplesize = buf[18]<<8 | buf[19];
        uint16 const pre_defined = buf[20]<<8 | buf[21];
        uint16 const reserved3 = buf[22]<<8 | buf[23];
        //uint32 const samplerate = buf[24]<<8 | buf[25]; // 16.16 fixed point format
        if (0!=reserved1 || 0!=reserved2 || 0!=pre_defined || 0!=reserved3) {
            BL_ERR("AudioSampleEntry check(%s) - first 2 dwords:(0x%X, 0x%X) pre_defined:%d reserved(word):%d should be all 0s\n",
                    name, reserved1, reserved2, pre_defined, reserved3);
        }
        //BL_LOG("AudioSampleEntry check(%s) - sample version:%d %d channels, sample size:%d, sample rate:%d\n",
        //        name, sample_desc_version, channel_count, samplesize, samplerate);
#endif
        if (0!=sample_desc_version) {
            if (1==sample_desc_version) {
                // 16 bytes followed.
                //  uint32 samplesPerPkt;   [0:3]
                //  uint32 bytesPerPkt;     [4:7]
                //  uint32 bytesPerFrame;   [8:11]
                //  uint32 bytesPerSample;  [12:15]
                _fseeki64(file, 16, SEEK_CUR);
                read_bytes += 16;
            }
            else if (2==sample_desc_version) {
                // 36 bytes follow ffmpeg/libavformt/mov.c, ff_mov_read_stsd_entries() line 2239
                //  uint32 size_of_struct;  // [0:3]
                //  uint64 sample_rate;     // [4:11], double
                //  uint32 channel_count;   // [12:15]
                //  uint32 const magic_no;  // [16:19], { 0x7f, 0x00, 0x00, 0x00 }
                //  uint32 const sample_size; // [20:23]
                //  uint32 LPCM_Flags;      // [24:27]
                //  uint32 bytes_per_frame; // [28:31]
                //  uint32 samplePerPkt;    // [32:35]
                if (36==fread(buf, 1, 36, file)) { /*
                    union int64_double {
                        int64_t ii;
                        double oo;
                    } sr;
                    sr.ii = (buf[4]<<24 | buf[5]<<16 | buf[6]<<8 | buf[7]);
                    sr.ii = (sr.ii<<32) | (buf[8]<<24 | buf[9]<<16 | buf[10]<<8 | buf[11]);
                    double sample_rate = (*(double*) &sr); // or simply...
*/
                    if (0x7f000000==(buf[16]<<24 | buf[17]<<16 | buf[18]<<8 | buf[19])) {
                        channel_count = (buf[12]<<24 | buf[13]<<16 | buf[14]<<8 | buf[15]);
                    }
                }
                read_bytes += 36;
            }
            else { // not support
                BL_LOG("Unsupported audio sample(:%s) description version%d\n", name, sample_desc_version);
                _fseeki64(file, size-28, SEEK_CUR);
                return true;
            }
        }

/*
https://github.com/google/spatial-media/blob/master/docs/spatial-audio-rfc.md
aligned(8) class NonDiegeticAudioBox extends Box(．SAND・) {
    unsigned int(8) version; // must be 0
}
aligned(8) class SpatialAudioBox extends Box('SA3D') {
    unsigned int(8)  version;           // must be 0
    unsigned int(8)  ambisonic_type;    // 0 = Periphonic: Indicates that the audio stored is a periphonic ambisonic sound field
    unsigned int(32) ambisonic_order;   // If the ambisonic_type is 0 (periphonic)
    unsigned int(8)  ambisonic_channel_ordering;
    unsigned int(8)  ambisonic_normalization;
    unsigned int(32) num_channels;     // big endian
    for (i = 0; i < num_channels; i++) {
        unsigned int(32) channel_map;
    }
}
*/
        buf[8] = '\0';
        uint8* a3d = buf + 9;
        while ((read_bytes+8)<=size && 8==fread(buf, 1, 8, file)) {
            uint32 const boxSize = ((uint32)buf[0])<<24 | buf[1]<<16 | buf[2]<<8 | buf[3];
            uint32 const contentSize = boxSize - 8;
            if (0==memcmp(buf+4, "SAND", 4) && 1==contentSize) {
                uint32 const rsize = (uint32) fread(a3d, 1, contentSize, file);
                if (rsize==contentSize && 0==a3d[0] && 2==channel_count) {
                    sand_ = 1;
                }
                read_bytes += (8 + rsize);
                break;
            }
            else if (min_SA3D_size<=contentSize && contentSize<=max_SA3D_size && 0==memcmp(buf+4, "SA3D", 4)) {
                uint32 const rsize = (uint32) fread(a3d, 1, contentSize, file);
                if (contentSize==rsize) {
                    int const ambisonic_order = (a3d[2]<<24)|(a3d[3]<<16)|(a3d[4]<<24)|a3d[5];
                    int const sa3d_num_channels = (a3d[8]<<24)|(a3d[9]<<16)|(a3d[10]<<8)|a3d[11];
                    BL_LOG("SA3D box(%s) : version=%d(must be 0) type=%d(0:Periphonic): order=%d(1:FOA) channel_ordering=%d(0:ACN) normalization=%d(0:SN3D) #(channels)=%d(4:FOA)\n",
                           name,
                           a3d[0], // version must be 0
                           a3d[1], // ambisonic_type, 0 = Periphonic(Full 3D)
                           ambisonic_order, // ambisonic_order, must be 1 currently
                           a3d[6], // ambisonic_channel_ordering, must be 0(ACN) currently
                           a3d[7], // ambisonic_normalization, 0 = SN3D
                           sa3d_num_channels // num_channels
                           );

                    if (AUDIO_TECHNIQUE_DEFAULT!=sa3d_.Technique) {
                        BL_LOG("!!! overwrite SA3D(%d) !!!\n", sa3d_.Technique);
                    }

                    sa3d_.Reset();

                    if (0==a3d[0] && 0==a3d[1] && 0<ambisonic_order && ambisonic_order<=3 &&
                        0==a3d[6] && sa3d_num_channels==((ambisonic_order+1)*(ambisonic_order+1))) {
                        sa3d_.Technique = AUDIO_TECHNIQUE_AMBIX;
                        if (0==a3d[7]) {
                            // SN3D
                        }
                        sa3d_.TotalStreams  = 1;
                        sa3d_.TotalChannels = sa3d_.ChannelCnt[0] = (uint8) sa3d_num_channels;
                        sa3d_.StreamIds[0] = (uint8) (parsing_track_no_ - 1);

                        uint8 const* map = a3d + 12;
                        uint8 map_check[sizeof(sa3d_.Indices)];
                        memset(map_check, 0xff, sizeof(map_check));
                                                
                        for (int i=0; i<sa3d_num_channels; ++i,map+=4) {
                            if (0==map[0] && 0==map[1] && 0==map[2] && map[3]<sizeof(sa3d_.Indices) &&
                                0xff==map_check[map[3]]) {
                                sa3d_.Indices[i] = map[3];
                                map_check[map[3]] = 1;
                            }
                            else {
                                sa3d_.Reset();
                                break;
                            }
                        }
                    }
                }

                read_bytes += (8 + rsize);
                break;
            }
            else {
                _fseeki64(file, contentSize, SEEK_CUR);
            }
 
            read_bytes += boxSize;
        }

        //assert(read_bytes==size);
        if (read_bytes<=size) {
            if (read_bytes<size) {
                _fseeki64(file, size-read_bytes, SEEK_CUR);
            }
            return true;
        }

        return false;
    }

    bool moov_trak_mdia_minf_(FILE* file, uint32 size, uint32 media_type, int level=0) {
        uint32 const video_type = BL_MAKE_4CC('v', 'i', 'd', 'e');
        uint32 const audio_type = BL_MAKE_4CC('s', 'o', 'u', 'n');
        if (0==level) { 
            if (audio_type==media_type) {
                if (AUDIO_TECHNIQUE_DEFAULT!=sa3d_.Technique) {
                    _fseeki64(file, size, SEEK_CUR);
                    return true;
                }
            }
            else if (video_type==media_type) {
                if (htc::VIDEO_SOURCE_GOOGLE_JUMP_RFC2==metadata_source_) {
                    _fseeki64(file, size, SEEK_CUR);
                    return true;
                }
            }
            else {
                _fseeki64(file, size, SEEK_CUR);
                return true;
            }
        }

        uint8 hdr[9]; hdr[8] = '\0';
        uint32 read_bytes = 0;
        while (read_bytes<size && 8==fread(hdr, 1, 8, file)) {
            uint32 const boxSize = BL_MAKE_4CC(hdr[0], hdr[1], hdr[2], hdr[3]);
            if (boxSize<=8) {
                if (8==boxSize) {
                    read_bytes += 8;
                    continue;
                }
                else {
                    return false;
                }
            }

            if (0==level) {
                if (0==memcmp(hdr+4, "stbl", 4) && boxSize>8) {
                    if (!moov_trak_mdia_minf_(file, boxSize-8, media_type, 1))
                        return false;
                }
                else {
                    _fseeki64(file, boxSize-8, SEEK_CUR);
                }
            }
            else if (1==level) {
                if (0==memcmp(hdr+4, "stsd", 4) && boxSize>8) {
                    //
                    // 4 bytes version/flags = byte hex version + 24-bit hex flags (FullBox)
                    // + 4 bytes number of descriptions = long unsigned total
                    //
                    _fseeki64(file, 8, SEEK_CUR);
                    if (!moov_trak_mdia_minf_(file, boxSize-16, media_type, 2))
                        return false;
                }
                else {
                    _fseeki64(file, boxSize-8, SEEK_CUR);
                }
            }
            else if (2==level) {
                if (video_type==media_type) {
                    // spherical video RFC2 = "moov.trak.mdia.minf.stbl.stsd.****.sv3d"
                    // spherical video RFC2 = "moov.trak.mdia.minf.stbl.stsd.****.st3d"
                    // (**** = avc1, avc2 or hvc1)
                    if (!moov_trak_mdia_minf_stbl_stsd_VideoSampleEntry(file, hdr+4, boxSize-8))
                        return false;
                }
                else {
                    // spherical audio = "moov.trak.mdia.minf.stbl.stsd.****.SA3D"
                    assert(audio_type==media_type);
                    if (!moov_trak_mdia_minf_stbl_stsd_AudioSampleEntry(file, hdr+4, boxSize-8))
                        return false;
                }
            }
            else {
                _fseeki64(file, boxSize-8, SEEK_CUR);
            }
 
            read_bytes += boxSize;
        }
        return (read_bytes==size);
    }

    bool moov_trak_mdia_(FILE* file, uint32 size) {
        uint8 hdr[9]; hdr[8] = '\0';
        uint32 read_bytes = 0;
        uint32 mdeia_type = 0;
        while (read_bytes<size && 8==fread(hdr, 1, 8, file)) {
            uint32 const boxSize = BL_MAKE_4CC(hdr[0], hdr[1], hdr[2], hdr[3]);
            if (boxSize<=8) {
                if (8==boxSize) {
                    read_bytes += 8;
                    continue;
                }
                else {
                    return false;
                }
            }

            if (0==memcmp(hdr+4, "minf", 4)) {
                if (!moov_trak_mdia_minf_(file, boxSize-8, mdeia_type))
                    return false;
            }
            else if (0==memcmp(hdr+4, "hdlr", 4)) {
/*                    
                aligned(8) class HandlerBox extends FullBox("hdlr", version = 0, 0) {
                    unsigned int(32) pre_defined = 0;
                    unsigned int(32) handler_type;
                    const unsigned int(32)[3] reserved = 0;
                    string name;
                }
*/
                // [0:3] FullBox's version_and_flags.
                // [4:7] pre_defined
                // [8:11] handler type
                uint32 hdlr_read_size = 0;
                if (boxSize>30 && 0==mdeia_type) {
                    char data[12];
                    hdlr_read_size = (uint32) fread(data, 1, 12, file);
                    if (12==hdlr_read_size) {
                        mdeia_type = BL_MAKE_4CC(data[8], data[9], data[10], data[11]);
                    }
                }
                _fseeki64(file, boxSize-8-hdlr_read_size, SEEK_CUR);
            }
            else {
                _fseeki64(file, boxSize-8, SEEK_CUR);
            }
            read_bytes += boxSize;
        }
        return (read_bytes==size);
    }

    // 2017/02/24 parse metadata for FB360 audio(TBE+stereo)
    // 'meta' box see 14496-12 8.44.1.2
    bool meta_box_(FILE* file, uint32 size, int track_no/*1 base*/) {
        //
        // meta box should be a FullBox... the current 4 bytes should be
        // uint8  version;
        // uint24 flags
        //
        // but some nasty files ain't so...
        //
        uint8 hdr[12];
        if (size>12 && 12==fread(hdr, 1, 12, file)) {
            uint32 read_bytes = 12;
            uint32 xml_box_size = 0;
            if (0!=memcmp(hdr+4, "hdlr", 4)) { // normal case
                uint32 const boxSize = BL_MAKE_4CC(hdr[4], hdr[5], hdr[6], hdr[7]);
                if (0==memcmp(hdr+8, "xml ", 4)) { // normally this should be 'hdlr' box
                    xml_box_size = boxSize; // impossible?
                }
                else {
                    _fseeki64(file, boxSize-8, SEEK_CUR); // bypass 'hdlr' box
                    read_bytes = 4 + boxSize;
                }
            }
            else { // bad case - short of 4 bytes
                uint32 const boxSize = BL_MAKE_4CC(hdr[0], hdr[1], hdr[2], hdr[3]);
                _fseeki64(file, boxSize-12, SEEK_CUR); // bypass 'hdlr' box
                read_bytes = boxSize;
            }

            while (0==xml_box_size && read_bytes<size && 8==fread(hdr, 1, 8, file)) {
                uint32 const boxSize = BL_MAKE_4CC(hdr[0], hdr[1], hdr[2], hdr[3]);
                if (0==memcmp(hdr+4, "xml ", 4)) {
                    xml_box_size = boxSize;
                    break;
                }
                else {
                    _fseeki64(file, boxSize-8, SEEK_CUR);
                }
                read_bytes += boxSize;
            }

            if (xml_box_size>0) {
                char buffer[512];
                uint32 rsize(0), msize(xml_box_size-8);
                if (msize<sizeof(buffer)) {
                    rsize = (uint32) fread(buffer, 1, msize, file);
                    if (rsize==msize) {
                        buffer[rsize] = 0;
                        char const* xml_begin = buffer + 4; // FullBox's version(8) + flag(24)
                        char const* xml_end = buffer+rsize;
                        while ('<'!=*xml_begin && xml_begin<xml_end)
                            ++xml_begin;

                        while ('>'!=xml_end[-1] && xml_begin<xml_end)
                            --xml_end;

                        if ((xml_end-xml_begin)>8 && 0==memcmp("<fb360", xml_begin, 6)) {
                            if (track_no>0) { // 1-base
                                assert(track_no==parsing_track_no_);
                                //BL_LOG("moov.trak(#%d).meta.xml:\n%s\n\n", track_no, xml_begin);

                                //
                                // [workaround]
                                // "Tsai for venice -gear color final_TBH_H265_60mbps_FB360.mp4"
                                // no "moov.meta.xml"
                                if (AUDIO_TECHNIQUE_TBE!=sa3d_.Technique) {
                                    sa3d_.Reset();
                                    sa3d_.Technique = AUDIO_TECHNIQUE_TBE;
                                }

                                FB360Metadata FB360;
                                if (track_no<256 &&
                                    FB360.Parse(xml_begin, xml_end) && FB360.track_info>=0) {
                                    if (FB360.track_info<(sizeof(sa3d_.StreamIds)/sizeof(sa3d_.StreamIds[0])) &&
                                        0xff==sa3d_.StreamIds[FB360.track_info]) {
                                        sa3d_.StreamIds[FB360.track_info] = (uint8) (track_no - 1); // 1 based
                                        ++sa3d_.TotalStreams;
                                    }
                                }
                            }
                            else {
                                //BL_LOG("moov.meta.xml:\n%s\n\n", xml_begin);
                                sa3d_.Reset();
                                sa3d_.Technique = AUDIO_TECHNIQUE_TBE;
                            }
                        }
                    }
                }

                if (rsize<msize) {
                    _fseeki64(file, msize-rsize, SEEK_CUR);
                }

                read_bytes += xml_box_size;

                if (read_bytes<size) {
                    _fseeki64(file, size-read_bytes, SEEK_CUR);
                }
            }

            return (read_bytes==size);
        }
        return false;
    }
    bool moov_trak_(FILE* file, uint32 size) {
        uint8 hdr[9]; hdr[8] = '\0';
        uint32 read_bytes = 0;
        while (read_bytes<size && 8==fread(hdr, 1, 8, file)) {
            uint32 const boxSize = BL_MAKE_4CC(hdr[0], hdr[1], hdr[2], hdr[3]); // big endian
            //BL_LOG("box:\"moov.trak.%s\" start:%d size:%d bytes\n", (char const*)hdr+4, (int)ftell(file), boxSize-8);
            if (0==memcmp(hdr+4, "mdia", 4)) {
                if (!moov_trak_mdia_(file, boxSize-8))
                    return false;
            }
            else if (0==memcmp(hdr+4, "meta", 4)) { // track metadata...
                if (!meta_box_(file, boxSize-8, parsing_track_no_))
                    return false;
            }
            else if (0==memcmp(hdr+4, "uuid", 4) &&
                htc::VIDEO_SOURCE_UNKNOWN==metadata_source_ && boxSize>408) { // spherical video = "moov.trak.uuid"
                uint8 uuid[16];
                if (16!=fread(uuid, 1, 16, file)) // a great guard
                    return false;

                uint32 const xml_size = boxSize - 8 - 16;
                if (0==memcmp(uuid, SPHERICAL_UUID_ID, 16) && xml_size<4096) {
                    longitude_ = latitude_south_ = latitude_north_ = 0;
                    void* const buf = malloc(xml_size);
                    if (NULL!=buf) {
                        if (xml_size==fread(buf, 1, xml_size, file)) {
                            char const* xml_begin = (char const*) buf;
                            char const* xml_end = xml_begin + xml_size;

                            // may have header if made by Google's "Spherical Metadata Tool".
                            char const* xml_header = "<?xml version=\"1.0\"?>"; // length = 21
                            if (0==memcmp(xml_begin, xml_header, 21))
                                xml_begin += 21;

                            while ('<'!=*xml_begin && xml_begin<xml_end)
                                ++xml_begin;

                            while ('>'!=xml_end[-1] && xml_begin<xml_end)
                                --xml_end;

                            GoogleJumpMetadata GJump;
                            if (GJump.Parse(xml_begin, xml_end) &&
                                GJump.spherical.GetViewCoverage(longitude_,
                                                                latitude_south_,
                                                                latitude_north_)) {
                                metadata_source_ = htc::VIDEO_SOURCE_GOOGLE_JUMP;
                                stereo_mode_ = GJump.stereo_mode;
                            }
                        }
                        else {
                            free(buf);
                            return false;
                        }
                        free(buf);
                    }
                    else {
                        _fseeki64(file, xml_size, SEEK_CUR);
                    }
                }
                else {
                    _fseeki64(file, xml_size, SEEK_CUR);
                }
            }
            else {
                _fseeki64(file, boxSize-8, SEEK_CUR);
            }

            read_bytes += boxSize;
        }
        return (read_bytes==size);
    }
    bool moovBox_(FILE* file, uint64 size) {
        uint8 hdr[9]; hdr[8] = '\0';
        uint32 read_bytes = 0;
        parsing_track_no_ = 0;
        while (read_bytes<size && 8==fread(hdr, 1, 8, file)) {
            uint32 boxSize = BL_MAKE_4CC(hdr[0], hdr[1], hdr[2], hdr[3]); // big endian
            //BL_LOG("box:\"moov.%s\" size:%d bytes\n", (char const*)hdr+4, boxSize-8);
            if (0==memcmp(hdr+4, "trak", 4)) {
                ++parsing_track_no_;
                if (!moov_trak_(file, boxSize-8))
                    return false;
            }
            else if (0==memcmp(hdr+4, "meta", 4)) {
                if (!meta_box_(file, boxSize-8, -1))
                    return false;
            }
            else {
                _fseeki64(file, boxSize-8, SEEK_CUR);
            }
            read_bytes += boxSize;
        }
        moov_ = (read_bytes==size);

        // verify FB360
        if (AUDIO_TECHNIQUE_TBE==sa3d_.Technique) {
            if ((2==sa3d_.TotalStreams || 3==sa3d_.TotalStreams) &&
                0xff!=sa3d_.StreamIds[0] &&
                0xff!=sa3d_.StreamIds[1]) {
                sa3d_.ChannelCnt[0] = sa3d_.ChannelCnt[1] = 4;
                if (0xff!=sa3d_.StreamIds[2]) {
                    sa3d_.TotalStreams = 3;
                    sa3d_.TotalChannels = 10;
                    sa3d_.ChannelCnt[2] = 2;
                }
                else {
                    sa3d_.TotalStreams = 2;
                    sa3d_.TotalChannels = 8;
                }

                ///////////////////////////////////////////////////////////////////////////
                // this is not necessary, but if decoder try not to decode all 3 streams,
                // but use 4-channel "tbe_a" stream instead, the first 4 channel indices
                // may be useful.
                //
                // The conversion formulas from Ambix to TBE, obtained by trial and errors :
                // http://pcfarina.eng.unipr.it/TBE-conversion.htm
                //  TBE[0] =  0.486968 * Ambix[0]
                //  TBE[1] = -0.486968 * Ambix[1] // !!! negative !!!!
                //  TBE[2] =  0.486968 * Ambix[3]
                //  TBE[3] =  0.344747 * Ambix[2] + 0.445656 * Ambix[6]
                //  TBE[4] = -0.630957 * Ambix[8]
                //  TBE[5] = -0.630957 * Ambix[4]
                //  TBE[6] = -0.630957 * Ambix[5]
                //  TBE[7] =  0.630957 * Ambix[7]
                //
                sa3d_.Indices[0] = 0; // W
                sa3d_.Indices[1] = 1; // Y !!! negative !!!!
                sa3d_.Indices[2] = 3; // Z
                sa3d_.Indices[3] = 2; // X

                sa3d_.Indices[4] = 5;
                sa3d_.Indices[5] = 6;
                sa3d_.Indices[6] = 3; // merge into Z, could be bad!!!
                sa3d_.Indices[7] = 7;
                sa3d_.Indices[8] = 4;
                ////////////////////////////////////////////////////////////////////////////
            }
            else {
                sa3d_.Reset();
            }
        }

        return (0!=moov_);
    }

    bool ReadBox_(FILE* file, uint8 const* name, uint64 size) {
        //BL_LOG("box:\"%s\" size:%lld bytes\n", (char const*) name, size);
        if (0==memcmp(name, "moov", 4)) {
            return moovBox_(file, size);
        }
        else if (0==memcmp(name, "mdat", 4)) {
            mdat_pos_  = _ftelli64(file);
            mdat_size_ = size;
        }
        else if (0==memcmp(name, "uuid", 4)) { // well... www.virtualrealporn.com... [caution!] NSFW, don't visit it.
                                               // these videos are likely be 180 + SBS3D
            int const max_parse_xmp_data = (256<<10); // give up if size not reasonable.
            if (size>16 && size<max_parse_xmp_data && htc::VIDEO_SOURCE_UNKNOWN==metadata_source_) {
                uint8 uuid[16];
                if (16!=fread(uuid, 1, 16, file))
                    return false;

                size -= 16;
                if (0==memcmp(uuid, XMP_UUID_ID, 16)) {
                    char* const data = (char*) malloc((int)size);
                    if (NULL!=data && size==fread(data, 1, (size_t)size, file)) {
                        char const* xml_begin = data;
                        char const* xml_end = data;
                        for (int i=11; i<size; ++i,++xml_begin) {
                            if (0==memcmp("<x:xmpmeta", xml_begin, 10)) {
                                char const* head = xml_begin + 24;
                                char* tail = data + size; // <?packet end="w"?>
                                while (head<tail) {
                                    if (0==memcmp("</x:xmpmeta>", tail-12, 12)) {
                                        *tail = '\0';
                                        xml_end = tail;
                                        break;
                                    }
                                    else {
                                        --tail;
                                    }
                                }
                                break;
                            }
                        }

                        if (xml_begin<xml_end) {
                            longitude_ = latitude_south_ = latitude_north_ = 0;
                            VirtueRealPornMetadata porn;
                            if (porn.Parse(xml_begin, xml_end) &&
                                porn.gpano.GetViewCoverage(longitude_,
                                                           latitude_south_,
                                                           latitude_north_)) {
                                metadata_source_ = htc::VIDEO_SOURCE_VIRTUALREAL_PORN;
                                stereo_mode_ = porn.stereo_mode;
                            }

                            //FILE* xmp = fopen("uuid.xmp", "wb");
                            //fwrite(xml_begin, 1, xml_end-xml_begin, xmp);
                            //fclose(xmp);
                        }
                    }

                    free(data);
                    size = 0;
                }
            }
        }

        if (size>0) {
            _fseeki64(file, size, SEEK_CUR);
        }

        return true;
    }

    // Matroska
    bool ParseEBML_VideoSettings_(FILE* file, uint64 vs_size) {
        // https://www.matroska.org/technical/specs/index.html#Video
        uint32 PixelWidth = 0;
        uint32 PixelHeight = 0;
        uint32 DisplayWidth = 0;
        uint32 DisplayHeight = 0;
        uint32 StereoMode = BL_BAD_UINT32_VALUE;

        uint64 read_bytes(0), size(0);
        uint32 a, b, header;
        while (read_bytes<vs_size) {
            header = EBML_element_header(a, file); read_bytes += a;
            size = EBML_element_size(b, file); read_bytes += b;
            if (size<=4) {
                switch (header)
                {
                case 0xb0:
                    PixelWidth = EBML_element_read_uint(file, (uint32) size);
                    break;

                case 0xba:
                    PixelHeight = EBML_element_read_uint(file, (uint32) size);
                    break;

                case 0x54b0:
                    DisplayWidth = EBML_element_read_uint(file, (uint32) size);
                    break;

                case 0x54ba:
                    DisplayHeight = EBML_element_read_uint(file, (uint32) size);
                    break;

                case 0x53b8:
                    StereoMode = EBML_element_read_uint(file, (uint32) size);
                    break;

                    //
                    // [TO-DO] Spherical Video V2 RFC - WebM (Matroska)
                    // Spherical video metadata is stored in a new master element,
                    // Projection, placed inside a video track's Video master element.
                    //
                    // As the V2 specification stores its metadata in a different location,
                    // it is possible for a file to contain both the V1 and V2 metadata.
                    // If both V1 and V2 metadata are contained they should contain semantically
                    // equivalent information, with V2 taking priority when they differ.
                case 0x7670: { // Projection master element
                        EBML_GoogleJump GJump;
                        if (!GJump.ParseV2(file, size))
                            return false;
                    }
                    break;

                default:
                    _fseeki64(file, size, SEEK_CUR);
                    break;
                }
            }
            else {
                _fseeki64(file, size, SEEK_CUR);
            }

            read_bytes += size;
        }

        if (vs_size==read_bytes && StereoMode<BL_BAD_UINT32_VALUE &&
            0<PixelWidth && 0<PixelHeight) {
            //
            // https://www.matroska.org/technical/specs/notes.html#3D
            //
            // 3D and multi-planar videos
            // There are 2 different ways to compress 3D videos: have each 'eye' track
            // in a separate track and have one track have both 'eyes' combined inside
            // (which is more efficient, compression-wise). Matroska supports both ways.
            // For the single track variant, there is the StereoMode element which
            // defines how planes are assembled in the track (mono or left-right combined).
            //
            // Odd values of StereoMode means the left plane comes first for more
            // convenient reading. The pixel count of the track (PixelWidth/PixelHeight)
            // should be the raw amount of pixels (for example 3840x1080 for full HD side by side)
            // and the DisplayWidth/Height in pixels should be the amount of pixels for one plane
            // (1920x1080 for that full HD stream).
            //
            switch (StereoMode)
            {
            case 1: // 1: side by side (left eye is first)
                stereo_mode_ = 2;
                if (PixelHeight==DisplayHeight && PixelWidth==(2*DisplayWidth)) {
                    full3D_ = 2;
                }
                break;

            case 2: // 2: top-bottom (right eye is first)
                stereo_mode_ = 3;
                if (PixelWidth==DisplayWidth && PixelHeight==(2*DisplayHeight)) {
                    full3D_ = 2;
                }
                break;

            case 3: // top-bottom (left eye is first)
                stereo_mode_ = 1;
                if (PixelWidth==DisplayWidth && PixelHeight==(2*DisplayHeight)) {
                    full3D_ = 2;
                }
                break;

            case 11: // side by side (right eye is first)
                stereo_mode_ = 4;
                if (PixelHeight==DisplayHeight && PixelWidth==(2*DisplayWidth)) {
                    full3D_ = 2;
                }
                break;

            case 0: // mono
            //case 4: // checkboard (right is first)
            //case 5: // checkboard (left is first)
            //case 6: // row interleaved (right is first)
            //case 7: // row interleaved (left is first)
            //case 8: // column interleaved (right is first)
            //case 9: // column interleaved (left is first)
            //case 10: // anaglyph (cyan/red)
            //case 12: // anaglyph (green/magenta)
            //case 13: // both eyes laced in one Block (left eye is first)
            //case 14: // both eyes laced in one Block (right eye is first)).
            default:
                stereo_mode_ = 0;
                full3D_ = 0;
                break;
            }
            return true;
        }
        assert(vs_size==read_bytes);
        return (vs_size==read_bytes);
    }
    bool ParseEBML_Tracks_(FILE* file, uint64 tracks_size, uint32 parent) {
        // https://www.matroska.org/technical/specs/index.html#Tracks
        uint64 read_bytes(0), size;
        uint32 a, b, header;
        while (read_bytes<tracks_size) {
            header = EBML_element_header(a, file); read_bytes += a;
            size = EBML_element_size(b, file); read_bytes += b;
            if (0x1654ae6b==parent && 0xae==header) { // TrackEntry
                ++parsing_track_no_;
                if (!ParseEBML_Tracks_(file, size, header))
                    return false;
            }
            else if (0xae==parent && 0xe0==header) { // video settings
                if (!ParseEBML_VideoSettings_(file, size))
                    return false;
            }
            else {
                _fseeki64(file, size, SEEK_CUR);
            }

            read_bytes += size;
        }

        return (tracks_size==read_bytes);
    }
    bool ParseEBML_Tags_(FILE* file, uint64 tags_size, uint32 parent) {
        uint64 read_bytes(0), size;
        uint32 a, b, header;
        while (read_bytes<tags_size) {
            header = EBML_element_header(a, file); read_bytes += a;
            size = EBML_element_size(b, file); read_bytes += b;
            if (0x1254c367==parent && 0x7373==header) {
                EBML_GoogleJump GJump;
                if (!GJump.Parse(file, size))
                    return false;

                if (GJump.IsValid()) {
                    metadata_source_ = htc::VIDEO_SOURCE_GOOGLE_JUMP;
                    longitude_ = GJump.longitude;
                    latitude_south_ = GJump.latitude_south;
                    latitude_north_ = GJump.latitude_north;
                    stereo_mode_ = GJump.stereo_mode;

                    read_bytes += size;
                    if (read_bytes<tags_size) {
                        _fseeki64(file, tags_size - read_bytes, SEEK_CUR);
                    }
                    return true;
                }
            }
            else {
                 _fseeki64(file, size, SEEK_CUR);
            }

            read_bytes += size;
        }

        return (tags_size==read_bytes);
    }
    bool ParseEBML_Segment_(FILE* file, uint64 segment_size) {
        metadata_source_ = htc::VIDEO_SOURCE_UNKNOWN;
        uint64 read_bytes(0), size;
        uint32 a, b, header;
        int const max_trials = 2048; 
        for (int i=0; i<max_trials&&read_bytes<segment_size; ++i) {
            header = EBML_element_header(a, file); read_bytes += a;
            size = EBML_element_size(b, file); read_bytes += b;
            if (0x1654ae6b==header) { // Tracks, search for Stereo3D data
                if (!ParseEBML_Tracks_(file, size, header))
                    return false;
            }
            else if (0x1254c367==header) { // Tags, search for spherical video metadata
                if (!ParseEBML_Tags_(file, size, header))
                    return false;

                if (htc::VIDEO_SOURCE_GOOGLE_JUMP==metadata_source_)
                    return true;
            }
            else {
                // 'SeekHead' = 0x114D9B74
                // 'Void'     = 0x000000EC
                // 'Info'     = 0x1549A966
                // 'Cluster'  = 0x1F43B675
                // 'Cues'     = 0x1c53bb6b
                // 'Attachments' = 0x1941a469
                // 'Chapters'    = 0x1043a770
                //BL_LOG("0x%08X size=%d\n", header, size);
                _fseeki64(file, size, SEEK_CUR);
            }
            read_bytes += size;
        }

        return (segment_size==read_bytes);
    }

public:
    VideoMetadata():
        file_size_(0),
        mdat_pos_(0),mdat_size_(0),
        nalu_length_size_(-1),
        parsing_track_no_(0),

        sa3d_(),

        metadata_source_(htc::VIDEO_SOURCE_UNKNOWN),
        longitude_(0),
        latitude_south_(0),
        latitude_north_(0),
        stereo_mode_(-1),

        ftyp_(0),
        moov_(0),
        full3D_(0),
        sand_(0)
        {}

    bool GetSphericalAngles(int& longi, int& lati_south, int& lati_north) const {
        if (longitude_>0 && -90<=latitude_south_ && latitude_north_<=90 &&
            latitude_south_<latitude_north_) {
            longi = longitude_;
            lati_south = latitude_south_;
            lati_north = latitude_north_;
            return true;
        }
        return false;
    }

    htc::SA3D const& SpatialAudio3D() const { return sa3d_; }
    htc::VIDEO_SOURCE Source() const { return metadata_source_; }
    int Stereo3D() const { return stereo_mode_; }
    uint8 Full3D() const { return full3D_; }

    bool Parse(FILE* file, uint64 file_size) {
        if (NULL==file || file_size<8)
            return false;

        file_size_ = file_size;
        mdat_pos_ = mdat_size_ = 0;
        nalu_length_size_ = -1;
        parsing_track_no_ = 0;

        sa3d_.Reset();

        metadata_source_ = htc::VIDEO_SOURCE_UNKNOWN;
        longitude_ = latitude_south_ = latitude_north_ = 0;
        stereo_mode_ = -1;

        ftyp_ = moov_ = full3D_ = 0;

        // MP4 boxes:
        // container types : "moov", "udta", "meta", "trak", "mdia, "minf", "stbl", "stsd", "uuid"
        // leaf types : "stco", "co64", "free", "mdat", "xml ", "hdlr", "ftyp", "esds", "soun", "SA3D"

        uint8 hdr[9]; hdr[8] = '\0';
        if (4!=fread(hdr, 1, 4, file)) {
            return false;
        }

        // check EBML header, for Matroska videos
        if (0x1a==hdr[0] && 0x45==hdr[1] && 0xdf==hdr[2] && 0xa3==hdr[3]) {
            ftyp_ = 2;

            uint32 a, b;
            uint64 const header_size = EBML_element_size(a, file);
            if (header_size>0) _fseeki64(file, header_size, SEEK_CUR);

            if (4==fread(hdr, 1, 4, file) &&
                0x18==hdr[0] && 0x53==hdr[1] && 0x80==hdr[2] && 0x67==hdr[3]) {
                uint64 const segment_size = EBML_element_size(b, file);
                uint64 const main_body_size = segment_size + header_size + a + b + 8;
                //if (file_size_==main_body_size) { // favor Zootop720p.HDTC.x264.AC3.Exclusive-CPG.mkv
                if (file_size_>=main_body_size) {
                    if (ParseEBML_Segment_(file, segment_size)) {
                        return true;
                    }
                }
            }

            return true;

            //return false;
        }

        // could it be "RIFF"? divx?
        if (0==memcmp(hdr, "RIFF", 4)) {
            // WAV (RIFF) audio header has 44 bytes
            uint8 riff_header[40];
            //uint8* p = riff_header;
            bool result = (40==fread(riff_header, 1, 40, file));
/*
            // size in little endian!?
            if (result) {
                uint32 file_size = 8 + (p[3]<<24 | p[2]<<16 | p[1]<<8 | p[0]); p+=4;
                result = (file_size==file_size_);
            }

            // TO-DO : parse this file...
            // http://www.topherlee.com/software/pcm-tut-wavformat.html
            if (result) {
                if (0!=memcmp(p, "WAVE", 4)&&0!=memcmp(p, "AVI ", 4)) {
                    return false;
                }
                p += 4;

                uint32 a, b;
                uint32 header = EBML_element_header(a, file);
                uint64 size = EBML_element_size(b, file);
                if (0x1a45dfa3==header) {
                }
            }

            if (result) {
                ftyp_ = 3;
            }
*/
            ftyp_ = 3;
            return result;
        }

        // H.264
        if ((4==fread(hdr+4, 1, 4, file) && 0==memcmp(hdr+4, "ftyp", 4))) {
            ftyp_ = 1;

            // skip ftyp box
            uint64 boxSize = BL_MAKE_4CC(hdr[0], hdr[1], hdr[2], hdr[3]);
            uint64 read_bytes = boxSize;
            if (boxSize>8) _fseeki64(file, boxSize-8, SEEK_CUR);

            // read all boxes
            while (read_bytes<file_size_) {
                if (8==fread(hdr, 1, 8, file)) {
                    uint64 header_size = 8;
                    boxSize = BL_MAKE_4CC(hdr[0], hdr[1], hdr[2], hdr[3]); // big endian
                    if (1==boxSize) {
                        uint8 sizeQ[8];
                        //assert(0==memcmp(hdr+4, "mdat", 4));
                        if (8==fread(sizeQ, 1, 8, file)) {
                            header_size += 8;
                            boxSize = BL_MAKE_4CC(sizeQ[0], sizeQ[1], sizeQ[2], sizeQ[3]);
                            boxSize = (boxSize<<32) | BL_MAKE_4CC(sizeQ[4], sizeQ[5], sizeQ[6], sizeQ[7]);
                        }
                        else {
                            return false;
                        }
                    }

                    if (boxSize>=header_size && ReadBox_(file, hdr+4, boxSize-header_size)) {
                        read_bytes += boxSize;
                    }
                    else {
                        return false;
                    }
                }
                else {
                    return false;
                }
            }

            //assert(read_bytes==file_size_);
            if (0==mdat_size_ || 0==moov_)
                return false;

            // google jump specified stereo mode
            if (htc::VIDEO_SOURCE_GOOGLE_JUMP==metadata_source_)
                return true;

            //
            // Important Note -andre 2016.06.30
            // There are still mysterious H264 bit stream that it can not be parsed
            // completely. In any case, it shouldn't crash the app...
            //
            // determine stereo mode - just test few bytes.
            // we would definitely not want to parse entire byte stream.
            // (No big deals if it fails)
#ifdef BL_DEBUG_BUILD
            int nalu_found = 0;
            //uint32 const max_check_bytes = 1024<<20; // 1GB
            uint32 const max_check_bytes = 4<<10; // 4KB
#else
            uint32 const max_check_bytes = 4<<10; // 4KB
#endif
            uint32 const check_bytes = (mdat_size_<max_check_bytes) ? ((uint32)mdat_size_):max_check_bytes;
            uint8* const h264 = (uint8*) malloc(check_bytes);
            if (NULL==h264)
                return true;

            _fseeki64(file, mdat_pos_, SEEK_SET);
            if (check_bytes!=fread(h264, 1, check_bytes, file)) {
                free(h264);
                return true;
            }

            int const stereo_mode_bak = stereo_mode_;
            stereo_mode_ = -1;
            uint8 const* ptr = h264;
            uint8 const* const end = h264 + check_bytes;

            // the first 4 bytes (start code) must be 00 00 00 01?
            if (annexB_startcode(h264)) {
                // Annex B
                ptr = (0x01==h264[2]) ? (h264+3):(h264+4);
                while (ptr<end) {
                    uint8 const* nal_start = ptr;
                    uint8 const* nal_end = nal_start;
                    while ((nal_end+4)<end && !annexB_startcode(nal_end)) {
                        ++nal_end;
                    }

                    if ((nal_end+4)>=end || nal_start>=nal_end || !annexB_startcode(nal_end)) {
                        break;
                    }

                    ptr = (0x01==nal_end[2]) ? (nal_end+3):(nal_end+4);

                    uint8 const nalu_type = *nal_start;
#ifdef BL_DEBUG_BUILD
                    BL_LOG("#%d nal_ref_idc:%d  nal_unit_type:%d  size:%d\n",
                        ++nalu_found, (nalu_type&0x60)>>5, nalu_type&0x1f, (nal_end - nal_start));
#endif
                    if (6==nalu_type) { // SEI message(0x06)
                        uint8 const* sei = nal_start + 1;
                        uint32 payloadType = 0;
                        uint32 payloadSize = 0;
                        while (sei<nal_end) {
                            if (sei_message(payloadType, payloadSize, sei, nal_end)) {
                                if (0x2d==payloadType && (sei+payloadSize)<nal_end) {
                                    // frame packing arrangement
                                    BitStream bs(sei, sei+payloadSize);
                                    uint32 v = bs.read_ue(); // frame_packing_arrangement_id, 0 ~ 2^32 -2
                                    v = bs.read_u1(); // frame_packing_arrangement_cancel_flag
                                    if (0==v) { // if (!frame_packing_arrangement_cancel_flag) {
                                        v = bs.read_bits(7); // frame_packing_arrangement_type
                                        if (3==v) {
                                            stereo_mode_ = 2; // side-by-side, STEREO_3D_LEFTRIGHT;
                                        }
                                        else if (4==v) { //top-bottom
                                            stereo_mode_ = 1; // top-bottom, STEREO_3D_TOPBOTTOM;
                                        }
                                    }
                                    break;
                                }

                                sei += payloadSize;
                            }
                            else {
                                break;
                            }
                        }

                        break;
                    }
                    else if (nalu_type&0x80) {
                        BL_ERR("forbidden_zero_bit is not zero!!!\n");
                        break;
                    }
                }
            }
            else {
                // AVC
                int NALU_length_size = nalu_length_size_; // -1(invalid), 1, 2, or 4
                if (NALU_length_size<0) {
                    BL_LOG("NAL size length not sure.... set it to 4\n");
                    NALU_length_size = 4;
                }

                while (ptr<end) {
                    uint32 nalu_size = *ptr++;
                    for (int i=1; i<NALU_length_size; ++i) {
                        if (ptr<end)
                            nalu_size = (nalu_size<<8) | (*ptr++);
                    }
#if 0
                    if ((0x21100500==nalu_size || 0x21000500==nalu_size) && (ptr+4)<end &&
                        0xa0==ptr[0] && 0x1b==ptr[1] && 0xff==ptr[2] && 0xc0==ptr[3]) {
                        break;
                    }
#endif
                    if ((ptr+nalu_size)>=end)
                        break;

                    uint8 const nalu_type = *ptr;
#ifdef BL_DEBUG_BUILD
                    BL_LOG("#%d nal_ref_idc:%d  nal_unit_type:%d  size:%d\n",
                            ++nalu_found, (nalu_type&0x60)>>5, nalu_type&0x1f, nalu_size);
#endif
                    if (6==nalu_type) { // SEI message(0x06)
                        uint8 const* sei = ptr + 1;
                        uint8 const* const sei_end = ptr + nalu_size;
                        uint32 payloadType = 0;
                        uint32 payloadSize = 0;
                        while (sei<sei_end) {
                            if (sei_message(payloadType, payloadSize, sei, sei_end)) {
                                if (0x2d==payloadType && (sei+payloadSize)<sei_end) {
                                    // frame packing arrangement
                                    BitStream bs(sei, sei+payloadSize);
                                    uint32 v = bs.read_ue(); // frame_packing_arrangement_id, 0 ~ 2^32 -2
                                    v = bs.read_u1(); // frame_packing_arrangement_cancel_flag
                                    if (0==v) { // if (!frame_packing_arrangement_cancel_flag) {
                                        v = bs.read_bits(7); // frame_packing_arrangement_type
                                        if (3==v) {
                                            stereo_mode_ = 2; // side-by-side, STEREO_3D_LEFTRIGHT;
                                        }
                                        else if (4==v) { //top-bottom
                                            stereo_mode_ = 1; // top-bottom, STEREO_3D_TOPBOTTOM;
                                        }
                                    }
                                    break;
                                }

                                sei += payloadSize;
                            }
                            else {
                                break;
                            }
                        }

                        break;
                    }
                    else if (nalu_type&0x80) {
                        BL_ERR("forbidden_zero_bit is not zero!!!\n");
                        break;
                    }

                    ptr += nalu_size;
                }
            }

            free(h264);
            if (-1==stereo_mode_)
                stereo_mode_ = stereo_mode_bak;

            return true;
        }

        //
        // not supporting format
        // NOTE : first 8 bytes had been read check hdr[8]
        //

        return true;
    }
};

namespace htc {

//---------------------------------------------------------------------------------------
bool VideoTrack::SetFilePath(wchar_t const* wfullpath,
                             char const* fullpath, int full_bytes,
                             char const* filename, int short_bytes)
{
    status_ = 0; liveStream_ = spatialAudioWithNonDiegetic_ = false;
    if (NULL!=wfullpath && NULL!=fullpath && NULL!=filename && short_bytes<=full_bytes) {
        duration_end_ = duration_ = timestamp_ = 0;

        // try open video file
        {
            AVFormatContext* formatCtx = NULL;
            int const error = avformat_open_input(&formatCtx, fullpath, NULL, NULL);
            if (0!=error) {
                char msg_buf[256];
                av_strerror(error, msg_buf, sizeof(msg_buf));
                BL_LOG("avformat_open_input(%s) failed! error:%s\n", filename, msg_buf);
                return false;
            }

            // for some codec, e.g. raw video .avi? must find stream info for duration!?
            if (AV_NOPTS_VALUE!=formatCtx->duration) {
                duration_ = (int) (formatCtx->duration/1000);
                assert((formatCtx->duration/1000)==duration_);
            }
            else {
                avformat_find_stream_info(formatCtx, NULL);
                if (AV_NOPTS_VALUE!=formatCtx->duration) {
                    duration_ = (int) (formatCtx->duration/1000);
                    assert((formatCtx->duration/1000)==duration_);
                }
            }
            avformat_close_input(&formatCtx);
        }
        duration_end_ = duration_;

        BL_ASSERT(NULL==fullpath_ && NULL==name_);
        blFree(name_); // utf8
        name_ = (char*) blMalloc(full_bytes + short_bytes);
        if (NULL==name_) {
            return false;
        }

        memcpy(name_, filename, short_bytes);

        fullpath_ = name_ + short_bytes;
        memcpy(fullpath_, fullpath, full_bytes);
        fullpath_bytes_ = full_bytes;

        // parse metadata
        FILE* file = _wfopen(wfullpath, L"rb");
        if (NULL==file) {
            // NOTE : By default, fopen filename is interpreted using the ANSI codepage(CP_ACP).
            // but fullpath is actually encoded in UTF-8.
            char tmp[512];
            WideCharToMultiByte(CP_ACP, 0, wfullpath, -1, tmp, 512, NULL, NULL);
            file = fopen(tmp, "rb");
            if (NULL==file) {
                file = fopen(fullpath, "rb");
            }

            if (NULL==file) {
                return false;
            }
        }

        _fseeki64(file, 0, SEEK_END);
        uint64 const file_size = _ftelli64(file);
        rewind(file);

        type_intrinsic_ = VIDEO_TYPE_2D;
        source_         = VIDEO_SOURCE_UNKNOWN;
        full3D_         = 0;
        sa3d_.Reset();

        VideoMetadata metadata;
        if (metadata.Parse(file, file_size)) {
            sa3d_ = metadata.SpatialAudio3D();
            source_ = metadata.Source();
            if (metadata.GetSphericalAngles(spherical_longitude_,
                                            spherical_latitude_south_,
                                            spherical_latitude_north_)) {
                switch (metadata.Stereo3D())
                {
                case 1:
                    type_intrinsic_ = VIDEO_TYPE_SPHERICAL_3D_TOPBOTTOM;
                    break;

                case 2:
                    type_intrinsic_ = VIDEO_TYPE_SPHERICAL_3D_LEFTRIGHT;
                    break;

                default: // default = mono
                    type_intrinsic_ = VIDEO_TYPE_SPHERICAL;
                    break;
                }
            }
            else {
                spherical_longitude_ = 0;
                spherical_latitude_south_ = 0;
                spherical_latitude_north_ = 0;
                switch (metadata.Stereo3D())
                {
                case 1:
                    type_intrinsic_ = VIDEO_TYPE_3D_TOPBOTTOM;
                    full3D_ = metadata.Full3D();
                    break;

                case 2:
                    type_intrinsic_ = VIDEO_TYPE_3D_LEFTRIGHT;
                    full3D_ = metadata.Full3D();
                    break;

                case 3:
                    type_intrinsic_ = VIDEO_TYPE_3D_BOTTOMTOP;
                    full3D_ = metadata.Full3D();
                    break;

                case 4:
                    type_intrinsic_ = VIDEO_TYPE_3D_RIGHTLEFT;
                    full3D_ = metadata.Full3D();
                    break;

                default: // default = mono
                    type_intrinsic_ = VIDEO_TYPE_2D;
                    break;
                }
            }
        }
        else {
            OutputDebugStringA("failed to parse \"");
            OutputDebugStringW(wfullpath);
            OutputDebugStringA("\" metadata\n");
        }
        fclose(file); file = NULL;

        // succeed
        type_ = type_intrinsic_;
        status_ = 1;
        return true;
    }
    return false;
}
//---------------------------------------------------------------------------------------
bool VideoTrack::SetLiveStream(char const* name, char const* url, uint32 timeout)
{
    status_ = 0; liveStream_ = false;
    if (NULL!=url && NULL!=name) {
        duration_end_ = duration_ = timestamp_ = 0;

        // try open video file
        AVFormatContext* formatCtx = NULL;
        int const error = avformat_open_input(&formatCtx, url, NULL, NULL);
        if (0!=error) {
            char msg_buf[256];
            av_strerror(error, msg_buf, sizeof(msg_buf));
            BL_LOG("avformat_open_input(%s) failed! error:%s\n", url, msg_buf);
            return false;
        }

        // for some codec, e.g. raw video .avi? must find stream info for duration!?
        if (formatCtx->duration>0 && AV_NOPTS_VALUE!=formatCtx->duration) {
            duration_ = (uint32) (formatCtx->duration/1000);
            assert((formatCtx->duration/1000)==duration_);
        }
        else {
            duration_ = 0xffffffff;
        }
        avformat_close_input(&formatCtx);

        duration_end_ = duration_;

        BL_ASSERT(NULL==fullpath_ && NULL==name_);
        blFree(name_); // utf8
        int const full_bytes = (int) strlen(url);
        int const short_bytes = (int) strlen(name);
        name_ = (char*) blMalloc(full_bytes + short_bytes + 2);
        if (NULL==name_) {
            return false;
        }

        memcpy(name_, name, short_bytes);
        name_[short_bytes] = '\0';

        fullpath_ = name_ + short_bytes + 1;
        memcpy(fullpath_, url, full_bytes);
        fullpath_[full_bytes] = '\0';
        fullpath_bytes_ = full_bytes;

        // sv3d
        type_ = type_intrinsic_ = VIDEO_TYPE_2D;
        spherical_longitude_ = 0;
        spherical_latitude_south_ = 0;
        spherical_latitude_north_ = 0;

        // sa3d
        //sa3d_.Reset();

        source_     = VIDEO_SOURCE_UNKNOWN;
        full3D_     = 0;
        status_     = 1;
        timeout_    = (timeout>250) ? timeout:250;
        liveStream_ = true;
        return true;
    }

    return false;
}

//---------------------------------------------------------------------------------------
static uint32 const crc_vivecinema  = mlabs::balai::CalcCRC("vivecinema");
static uint32 const crc_credits     = mlabs::balai::CalcCRC("credits");
static uint32 const crc_livestreams = mlabs::balai::CalcCRC("livestreams");
static uint32 const crc_videopaths  = mlabs::balai::CalcCRC("videopaths");
static uint32 const crc_videos      = mlabs::balai::CalcCRC("videos");

static uint32 const crc_180  = mlabs::balai::CalcCRC("180");
static uint32 const crc_180SBS  = mlabs::balai::CalcCRC("180SBS");
static uint32 const crc_180TB  = mlabs::balai::CalcCRC("180TB");
static uint32 const crc_360  = mlabs::balai::CalcCRC("360");
static uint32 const crc_360SBS  = mlabs::balai::CalcCRC("360SBS");
static uint32 const crc_360TB  = mlabs::balai::CalcCRC("360TB");
static uint32 const crc_SBS  = mlabs::balai::CalcCRC("SBS");
static uint32 const crc_TB  = mlabs::balai::CalcCRC("TB");

class VRVideoConfigParser : public XMLParser
{
    char buf_[1024];
    VRVideoConfig& config_;
    uint32 loading_;
    int stacks_;

    BL_NO_COPY_ALLOW(VRVideoConfigParser);

public:
    explicit VRVideoConfigParser(VRVideoConfig& config):config_(config),
        loading_(0),stacks_(0) {}

    bool BeginTag_(XML_Element const& ele, XML_Element const* ascent) {
        uint32 const tag = mlabs::balai::CalcCRC(ele.Tag);
        if (NULL!=ascent) {
            if (stacks_>1) {
                if (crc_credits==loading_) {
                    BL_LOG("credits %s %s / %s\n", ele.Tag,
                           ele.Attributes[0].Value, ele.Attributes[1].Value);
                }
                else if (crc_livestreams==loading_ || crc_videos==loading_) {
                    char const* name = NULL;
                    char const* url = NULL;
                    uint32 sv3d(0), sa3d(0), timeout(4000);
                    for (int i=0; i<ele.NumAttribs; ++i) {
                        XML_Attrib const& att = ele.Attributes[i];
                        if (NULL==name && 0==memcmp(att.Name, "name", 5)) {
                            name = att.Value;
                        }
                        else if (NULL==url && 0==memcmp(att.Name, "url", 4)) {
                            url = att.Value;
                        }
                        else if (0==sv3d && 0==memcmp(att.Name, "sv3d", 5)) {
                            uint32 const crc_sv3d = mlabs::balai::CalcCRC(att.Value);
                            if (crc_180==crc_sv3d) {
                                sv3d = 1;
                            }
                            else if (crc_180SBS==crc_sv3d) {
                                sv3d = 2;
                            }
                            else if (crc_180TB==crc_sv3d) {
                                sv3d = 3;
                            }
                            else if (crc_360==crc_sv3d) {
                                sv3d = 4;
                            }
                            else if (crc_360SBS==crc_sv3d) {
                                sv3d = 5;
                            }
                            else if (crc_360TB==crc_sv3d) {
                                sv3d = 6;
                            }
                            else if (crc_SBS==crc_sv3d) {
                                sv3d = 7;
                            }
                            else if (crc_TB==crc_sv3d) {
                                sv3d = 8;
                            }
                        }
                        else if (0==sa3d && 0==memcmp(att.Name, "sa3d", 5)) {
                            if (('F'==att.Value[0] || 'f'==att.Value[0]) &&
                                ('u'==att.Value[1]) && 
                                ('M'==att.Value[2] || 'm'==att.Value[2]) &&
                                ('a'==att.Value[3]) && ('\0'==att.Value[4])) {
                                sa3d = 1;
                            }
                            else if (('a'==att.Value[0]) && ('m'==att.Value[1]) && 
                                     ('b'==att.Value[2]) && ('i'==att.Value[3]) &&
                                     ('x'==att.Value[4] || 'X'==att.Value[4]) &&
                                     ('\0'==att.Value[5])) {
                                sa3d = 2;
                            }
                        }
                    }

                    if (crc_livestreams==loading_) {
                        config_.AddLiveStream(name, url, sv3d, sa3d, timeout);
                    }
                    else if (NULL!=url) {
                        // backslash to slash
                        int len = 0;
                        while (*url!='\0'&&len<sizeof(buf_)) {
                            char& c = buf_[len++] = *url++;
                            if (c=='\\')
                                c = '/';
                        }

                        if (len<sizeof(buf_)) {
                            buf_[len] = '\0';
                            config_.AddVideo(name, buf_, sv3d, sa3d);
                        }
                    }
                }
                else if (crc_videopaths==loading_) {
                    char const* name = NULL;
                    char const* url = NULL;
                    for (int i=0; i<ele.NumAttribs&&(NULL==name||NULL==url); ++i) {
                        XML_Attrib const& att = ele.Attributes[i];
                        if (NULL==name && 0==memcmp(att.Name, "name", 5)) {
                            name = att.Value;
                        }
                        else if (NULL==url && 0==memcmp(att.Name, "url", 4)) {
                            url = att.Value;
                        }
                    }

                    if (NULL!=url) {
                        // backslash to slash
                        int len = 0;
                        while (*url!='\0'&&len<sizeof(buf_)) {
                            char& c = buf_[len++] = *url++;
                            if (c=='\\')
                                c = '/';
                        }

                        if (len<sizeof(buf_)) {
                            buf_[len] = '\0';
                            config_.AddVideoPath(name, buf_);
                        }
                    }
                }
            }
            else {
                if (crc_vivecinema!=loading_) {
                    return false;
                }
                loading_ = tag;
            }
        }
        else {
            if (crc_vivecinema!=tag || 0!=stacks_)
                return false;

            loading_ = tag;
        }

        ++stacks_;
        return true;
    }
    bool EndTag_(XML_Element const& /*ele*/, XML_Element const* ascent) {
        --stacks_;
        //uint32 const tag = mlabs::balai::CalcCRC(ele.Tag);
        if (NULL!=ascent) {
            if (1==stacks_) {
                if (crc_credits==loading_) {
                    loading_ = crc_vivecinema;
                }
                else if (crc_livestreams==loading_) {
                    loading_ = crc_vivecinema;
                }
                else if (crc_videopaths==loading_) {
                    loading_ = crc_vivecinema;
                }
                else if (crc_videos==loading_) {
                    loading_ = crc_vivecinema;
                }
            }
        }
        else {
            assert(0==stacks_);
            return (crc_vivecinema==loading_);
        }

        return true;
    }
};

VRVideoConfig::VRVideoConfig(wchar_t const* xml):
stringPool_(),
liveStreams_(128),
videoPaths_(128),
videos_(128),
fails_(1)
{
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
        return;

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
            codepage = FindCodePage(uchardet_get_charset(chardet));
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

    // 2017.09.26 add app video path
    AddVideoPath("App Video", "$(APP_PATH)/video");
    AddVideoPath("App Video", "$(APP_PATH)/videos");

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

            VRVideoConfigParser parser(*this);
            if (parser.Parse(xml_begin, xml_end)) {
                fails_ = 0;
            }
        }
    }

    free(buf);
}

}