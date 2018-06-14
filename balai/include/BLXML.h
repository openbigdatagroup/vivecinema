/**
 *
 * HTC Corporation Proprietary Rights Acknowledgment
 * Copyright (c) 2012 HTC Corporation
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
 * @file	BLXML.h
 * @desc    xml parser and writer
 * @author	andre chen
 * @history	2012/01/31 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_XML_H
#define BL_XML_H

#include "BLArray.h"	// this #include "BLMemoey.h" 

namespace mlabs { namespace balai { namespace fileio {

// TO-DO :
// CDATA section : <![CDATA[ 
//                  .......................
//				    ]]>
//
// Processing Instruction : <?xml-stylesheet href="style.css" type="text/css" ?>
//
struct XML_Attrib {
	char const*	Name;
	char const*	Value;
};

struct XML_Element {
    char const*       Tag;
    char const*       Content; // don't read this if BeginTag_()
    XML_Attrib const* Attributes;
    int               NumAttribs;
};

inline char* xml_seek(char c, char* begin, char const* end) {
    while (c!=begin[0] && begin<end)
        ++begin;
    return begin;
}
inline bool xml_initial_check(char c) {
    return (!(c<'A'||c>'z'||(c>'Z'&&c<'a'))) || ('_'==c);
}
inline bool xml_junk_char(char c) {
    switch (c) 
    {
    case ' ':   // space doesn't mean a thing in xml
    case '\0':  // null-terminated character
                // (should never appear in xml file, but we will use it!)

// simple-escape-sequence(ISO/IEC 14882 2.13.2):
//  case '\'':  // single quote
//  case '\"':  // double quote(same as '"')
//  case '\?':  // question mark(same as '?')
//  case '\\':  // backslash
    case '\a':  // alert(bell)
    case '\b':  // backspace
    case '\f':  // form feed
    case '\n':  // new-line(line feed)
    case '\r':  // carriage return
    case '\t':  // horizontal tab
    case '\v':  // vertical tab
        return true;

    default:
        return false;
    }
}

//-----------------------------------------------------------------------------
// quick parser
//-----------------------------------------------------------------------------
typedef bool (*XML_BeginTagCallBack)(XML_Element const& ele, XML_Element const* ascent, void* user);
typedef bool (*XML_EndTagCallBack)(XML_Element const& ele, XML_Element const* ascent, void* user);

bool ParseXMLFile(char const* xmlFile, XML_BeginTagCallBack beginCallback, XML_EndTagCallBack endCallback, void* user);


//-----------------------------------------------------------------------------
// XMLParser class - do the xml parsing, remember we are not to load xml 
// completely, we just want to parse it, and get thing we only interested.
//-----------------------------------------------------------------------------
class FileRead;
class XMLParser
{
	// only happen when some tags have lengthy "content", notoriously COLLADA's
	//    <float_array id="floats" name="myFloats" count="1024">
	//       contain 1024s floats in text!!!...           ^^^^^
	//    </float_array>

	// buffer to read file
	enum { EXPAND_BUFFER_SIZE = 2048 };

	// result
	enum RESULT { 
		RESULT_ERROR,		// Oops!
		RESULT_USER_ABORT,	// callback return false
		RESULT_OK,			// fine!
		RESULT_BEGIN,		// tag begin, content should follow
		RESULT_EOF,			// End of file
	};

	// internal structures
	struct Attrib_ {
		uint32	Name;		// offset in the string pool
		uint32	Value;		// offset in the string pool
	};

	struct Element_ {
		uint32	Tag;		// offset in the string pool
		uint32	Attribs;    // start index to the xmlAttributes_
		uint32	NumAttribs; // #(Attributes)
		uint32	Content;	// offset in the string pool
	};

	// xml structures
	Array<Element_>  xmlElement_;
	Array<Attrib_>   xmlAttributes_;
	Array<XML_Attrib> xmlAttribsBuf_;

	// file
	FileRead* filePtr_;

	// read buffer
	char*	readBuffer_;
	uint32	readBufferSize_;
	uint32	readBufferByte_;
	uint32	readBufferPos_;
	uint32  readBufferEnd_;

	// temp string buffer
	char*   stringBuffer_;
	uint32  stringBufferSize_;
	uint32  stringWritePos_;

	// call back
	bool DoCallback_(Element_ const& element, Element_ const* pParent, bool EndTag);
	
	// read text from file, return RESULT_EOF/RESULT_ERROR/RESULT_OK
	RESULT FeedTextBuffer_();

	// extract text string for a tag
	RESULT ExtractTagText_();	
	
	// save temp string
	uint32 PushString_(char const* szStr, uint32 leng);
	uint32 AppendString_(char const* szStrCont, uint32 leng);

	// return RESULT_OK, RESULT_EOF or RESULT_OUTOFMEMORY
	RESULT BypassComments_();

	// parse text string
	RESULT ParseStartTag_();
	RESULT ParseEndTag_();
	RESULT ParseContent_();

	// 2 callbacks
	virtual bool BeginTagCallback_(XML_Element const& ele, XML_Element const* ascent) = 0;
	virtual bool EndTagCallback_(XML_Element const& ele, XML_Element const* ascent) = 0;

	// disable copy ctor and assignment operator
	BL_NO_COPY_ALLOW(XMLParser);

public:
	XMLParser();
	virtual ~XMLParser();
	bool ParseFile(char const* filename);
};

//-----------------------------------------------------------------------------
// XMLParserLite template class - this handy xml parser minimize memory
// allocation. try this, if you know the xml structure is simple.
//-----------------------------------------------------------------------------
template<int element_stacks=128, int attributes_stacks=256>
class XMLParserLite
{
    // caution : if xml structure is so complex, these 2 values may be too small to finish the job
    enum {
        MAX_XML_ELEMENTES  = element_stacks>128 ? element_stacks:128,
        MAX_XML_ATTRIBUTES = attributes_stacks>256 ? attributes_stacks:256,
    };

    // xml structures
    XML_Element xmlElements_[MAX_XML_ELEMENTES];
    XML_Attrib  xmlAttribs_[MAX_XML_ATTRIBUTES];

    // xml source buffer
    char* xml_ptr1_;
    char* xml_ptr2_;
    char* xml_tail_;
    int   xml_element_stack_;
    int   xml_attrib_stack_;

    // error code : 
    //   0 = proceed(OK)
    //  -1 = bad xml structure
    //  -2 = user abort
    //  -3 = MAX_XML_ELEMENTES too small
    //  -4 = MAX_XML_ATTRIBUTES too small
    int ExtractXMLTag_() {
        int error = BypassComments_();
        if (0!=error)
            return error;

        if (xml_ptr1_<xml_tail_) {
            if ('<'!=*xml_ptr1_)
                return -1;

            if ('>'==*xml_ptr2_)
                return 0;

            xml_ptr2_ = xml_seek('>', xml_ptr1_, xml_tail_);
            if (xml_ptr2_<xml_tail_ && (xml_ptr1_+1)<xml_ptr2_) {
                return 0;
            }
            return -1;
        }

        return ((0==xml_element_stack_) && (0==xml_attrib_stack_)) ? 1:-1;
    }
    int ParseStartTag_() {
        // skip something like : 
        //  excel XML : <?mso-application progid="Excel.Sheet"?>
        //              <?xml-stylesheet type="text/xsl" href="simple.xsl"?>
        // ...
        if ('?'==*xml_ptr1_) {
            if ('?'!=xml_ptr2_[-1])
                return -1;

            xml_ptr1_ = ++xml_ptr2_;
            return 0;
        }

        if (!xml_initial_check(*xml_ptr1_))
            return -1;

        XML_Element ele;
        char* tag   = xml_ptr1_ - 1; // need an extra space in case ln.264 may shift a byte
        ele.Tag     = tag;
        ele.Content = xml_tail_; // empty string, ""

        // terminate
        *xml_ptr2_ = '\0';
        for (; xml_ptr1_<xml_ptr2_; ++xml_ptr1_) {
            if (xml_junk_char(*xml_ptr1_)) {
                *xml_ptr1_ = '\0';
                break;
            }
        }
        memmove(tag, tag+1, xml_ptr1_-tag);

        // attributes
        ele.Attributes = xmlAttribs_ + xml_attrib_stack_;
        ele.NumAttribs = 0;

        uint32 count    = 0;
        uint32 count2   = 0;
        uint32 spaceEnd = 0;
        while (xml_ptr1_<xml_ptr2_) { // #attributes
            while (xml_ptr1_<xml_ptr2_ && xml_junk_char(*xml_ptr1_))
                ++xml_ptr1_;

            if (xml_ptr1_==xml_ptr2_ || (xml_ptr2_==(xml_ptr1_+1) && '/'==*xml_ptr1_))
                break;

            if (!xml_initial_check(*xml_ptr1_))
                return -1;

            // check for '='
            bool checkEq = true;
            char const* name = xml_ptr1_;
            for (count=0; xml_ptr1_!=xml_ptr2_; ++xml_ptr1_, ++count) {
                if (xml_junk_char(*xml_ptr1_)) {
                    *xml_ptr1_ = '\0';
                    break;
                }
                else if('='==*xml_ptr1_) {
                    checkEq = false;
                    *xml_ptr1_ = '\0';
                    break;
                }
            }
            if (xml_ptr2_==xml_ptr1_||0==count)
                return -1;

            // value
            char* value = xml_ptr1_;
            if (checkEq) {
                while (value!=xml_ptr2_ && xml_junk_char(*value))
                    ++value;
                if (xml_ptr2_==value || '='!=*value++)
                    return -1;
            }

            //
            // XML Attributes Must be Quoted -
            //      <person gender="female">
            //               or
            //      <person gender='female'>
            // 

            // search " or '
            while (value!=xml_ptr2_ && xml_junk_char(*value))
                ++value;
            if (xml_ptr2_==value || ('\"'!=*value && '\''!=*value))
                return -1;

            //
            // can be double-quote(") or single-quote('), but must be paired 
            // a real case -
            // <rdf:li
            //    stRef:instanceID="xmp.iid:50522465-f17d-c644-b2a8-5c34ebbdcc13"
            //    stRef:fromPart="time:1464825600000f254016000000d668908800000f254016000000"
            //    stRef:toPart="time:3226003200000f254016000000d668908800000f254016000000"
            //    stRef:filePath="They're hot"  <-- check this out!!!
            //    stRef:maskMarkers="None"/>
            //
            char const quote = *value++; 

            // note attribute must omit space, eg. following 2 tags are identical
            //  <unit name=" centimeter " />
            //  <unit name="centimeter" />
            while (value!=xml_ptr2_ && ' '==*value)
                ++value;

            if (xml_ptr2_==value)
                return -1;

            bool hitDq = false; // hit quote " or '
            for (xml_ptr1_=value, spaceEnd=count2=0; xml_ptr1_!=xml_ptr2_; ++xml_ptr1_, ++count2) {
                //if ('\"'==*xml_ptr1_ || '\''==*xml_ptr1_) {
                if (quote==*xml_ptr1_) {
                    *xml_ptr1_ = '\0';
                    hitDq = true;
                    break;
                }

                if (xml_junk_char(*xml_ptr1_))
                    ++spaceEnd;
                else
                    spaceEnd = 0;
            }
            if (!hitDq || spaceEnd>count2) 
                return -1;

            if (xml_attrib_stack_>=MAX_XML_ATTRIBUTES) {
                return -4;
            }

            XML_Attrib& attr = xmlAttribs_[xml_attrib_stack_++];
            attr.Name  = name;
            attr.Value = (spaceEnd==count2) ? xml_tail_:value;
            ++(ele.NumAttribs);
        }

        XML_Element const* parent = (xml_element_stack_>0) ? (xmlElements_+xml_element_stack_-1):NULL;
        if (!BeginTag_(ele, parent)) {
            return -2;
        }

        // self ended, e.g. <me_and_myself />
        if ('/'==*xml_ptr1_) {
            if (!EndTag_(ele, parent)) {
                return -2;
            }

            if (ele.NumAttribs>0) {
                xml_attrib_stack_ -= ele.NumAttribs;
                if (xml_attrib_stack_<0)
                    return -1;
            }

            xml_ptr1_ = ++xml_ptr2_; // skip /
            return 0;
        }

        if (xml_element_stack_>=MAX_XML_ELEMENTES) {
            return -3;
        }

        xmlElements_[xml_element_stack_++] = ele;

        //
        // to reach here, we are encounter either
        //   A) another(child) element or 
        //   B) content text. including annoying comments!
        //

        // bypass comment! 
        int error = BypassComments_();
        if (0!=error)
            return error;

        // case A)
        if ('<'==*xml_ptr1_)
            return 0;

        // 1) find '<'
        xml_ptr2_ = xml_ptr1_ + 1;
        uint32 leng = 1;
        spaceEnd = 0;
        while (xml_ptr2_<xml_tail_) {
            if ('<'==*xml_ptr2_)
                break;

            if (xml_junk_char(*xml_ptr2_)) {
                *xml_ptr2_ = ' ';
                ++spaceEnd;
            }
            else {
                spaceEnd = 0; // ???
            }
            ++leng;
            ++xml_ptr2_;
        }

        if (spaceEnd>=leng)
            return 0; // <empty>   </empty>

        // need a '\0', shift 1 byte
        --xml_ptr1_;
        memmove(xml_ptr1_, xml_ptr1_+1, leng);
        char* append = xml_ptr2_ - 1;
        *append = '\0';
        xmlElements_[xml_element_stack_-1].Content = xml_ptr1_;
        xml_ptr1_ = xml_ptr2_;

        // wait!!!!
        // maybe readBufferPos_ start a comments section and there is "content" behind.
        // i.e.	 <tag>i love you <!-- comments -->, NOT!</tag>
        while (xml_ptr1_<xml_tail_) {
            error = BypassComments_();
            if (0!=error)
                return error;

            while (xml_ptr1_<xml_tail_ && xml_junk_char(*xml_ptr1_)) {
                ++xml_ptr1_;
            }

            if ('<'==*xml_ptr1_)
                return 0; // finish

            // ", NOT!!</tag>"
            char const* cont = xml_ptr1_;

            // find '<' again
            leng     = 1;
            spaceEnd = 0;
            while (++xml_ptr1_<xml_tail_) {
                if ('<'==*xml_ptr1_)
                    break;

                if (xml_junk_char(*xml_ptr1_)) {
                    *xml_ptr1_ = ' ';
                    ++spaceEnd;
                }
                else {
                    spaceEnd = 0;
                }
                ++leng;
            }

            if (spaceEnd>=leng)
                return 0; // spaces, never mind!

            memcpy(append, cont, (leng-spaceEnd));
            append[leng-spaceEnd] = '\0'; // null terminate?
        }

        return 0;
    }
    int ParseEndTag_() {
        if (xml_element_stack_<=0 || '/'!=*xml_ptr1_)
            return -1;

        XML_Element& ele = xmlElements_[--xml_element_stack_];

        char const* name = ++xml_ptr1_; // skip '/'
        *xml_ptr2_ = '\0';

        uint32 count = 0;
        for (; xml_ptr1_<xml_ptr2_; ++xml_ptr1_,++count) {
            if (xml_junk_char(*xml_ptr1_)) {
                *xml_ptr1_ = '\0';
                break;
            }
        }

        if (0==memcmp(ele.Tag, name, count+1)) {
            if (ele.NumAttribs>0) {
                xml_attrib_stack_ -= ele.NumAttribs;
                if (xml_attrib_stack_<0)
                    return -1;
            }

            XML_Element const* parent = (xml_element_stack_>0) ? (xmlElements_+xml_element_stack_-1):NULL;
            if (!EndTag_(ele, parent)) {
                return -2;
            }

            xml_ptr1_ = ++xml_ptr2_;

            return 0;
        }

        return -1;
    }
    int BypassComments_() {
        //
        // bypass comment section only, do not verify xml structure.
        //
        while (xml_ptr1_<xml_tail_ && xml_junk_char(*xml_ptr1_))
            ++xml_ptr1_;

        if ('<'!=*xml_ptr1_)
            return 0;
#if 1
        if ('!'==xml_ptr1_[1] && '-'==xml_ptr1_[2] && '-'==xml_ptr1_[3]) {
            xml_ptr1_ += 4;
            int stacks = 1;
            while (stacks>0 && xml_ptr1_<xml_tail_) {
                xml_ptr2_ = xml_seek('>', xml_ptr1_, xml_tail_);
                if (xml_ptr2_<xml_tail_) {
                    while ((xml_ptr1_=xml_seek('<', xml_ptr1_, xml_ptr2_))<xml_ptr2_) {
                        if ((xml_ptr1_+3)<xml_ptr2_ &&
                            '!'==xml_ptr1_[1] && '-'==xml_ptr1_[2] && '-'==xml_ptr1_[3]) {
                            ++stacks;
                            xml_ptr1_ += 4;
                        }
                        else {
                            ++xml_ptr1_;
                        }
                    }

                    xml_ptr1_ = xml_ptr2_ + 1;
                    if ('-'==xml_ptr2_[-1] && '-'==xml_ptr2_[-2]) {
                        if (0==--stacks) {
                            ++xml_ptr2_;
                            return BypassComments_();
                        }
                    }
                }
                else {
                    break;
                }
            }

            return -1;
        }
#else
        // <!--
        if ('!'==xml_ptr1_[1] && '-'==xml_ptr1_[2] && '-'==xml_ptr1_[3]) {
            xml_ptr2_ = seek('>', xml_ptr1_+4, xml_tail_);

            // -->
            if (xml_ptr2_<xml_tail_) {
                if ('-'==xml_ptr2_[-1] && '-'==xml_ptr2_[-2]) {
                    xml_ptr1_ = ++xml_ptr2_;
                    return BypassComments_();
                }
                else {
                    return 0;
                }
            }

            return -1;
        }
#endif

        return 0;
    }

    // 2 callbacks
    virtual bool BeginTag_(XML_Element const& ele, XML_Element const* ascent) = 0;
    virtual bool EndTag_(XML_Element const& ele, XML_Element const* ascent) = 0;

protected:
    XMLParserLite():xml_ptr1_(nullptr),xml_ptr2_(nullptr),xml_tail_(nullptr),
        xml_element_stack_(0),xml_attrib_stack_(0) {}
    virtual ~XMLParserLite() {}

public:
	// note end is the last ptr + 1
    bool Parse(char const* begin, char const* end) {
    /* quick test1
        begin = "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\" x:xmptk=\"Adobe XMP Core 5.1.0-jc003\">\n"
                "<rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">\n"
                "<rdf:Description rdf:about=\"\" xmlns:GPano=\"http://ns.google.com/photos/1.0/panorama/\">\n"
                "  <GPano:UsePanoramaViewer>True</GPano:UsePanoramaViewer>\n"
                "  <GPano:CaptureSoftware>HTC Pan360</GPano:CaptureSoftware>\n"
                "  <GPano:StitchingSoftware>SE.Visual.Panostitcher</GPano:StitchingSoftware>\n"
                "  <GPano:ProjectionType>equirectangular</GPano:ProjectionType>\n"
                "  <GPano:PoseHeadingDegrees>0.0</GPano:PoseHeadingDegrees>\n"
                "  <GPano:InitialHorizontalFOVDegrees>63.4</GPano:InitialHorizontalFOVDegrees>\n"
                "  <GPano:CroppedAreaImageWidthPixels>4096</GPano:CroppedAreaImageWidthPixels>\n"
                "  <GPano:CroppedAreaImageHeightPixels>2048</GPano:CroppedAreaImageHeightPixels>\n"
                "  <GPano:FullPanoWidthPixels>4096</GPano:FullPanoWidthPixels>\n"
                "  <GPano:FullPanoHeightPixels>2048</GPano:FullPanoHeightPixels>\n"
                "  <GPano:CroppedAreaLeftPixels>0</GPano:CroppedAreaLeftPixels>\n"
                "  <GPano:CroppedAreaTopPixels>0</GPano:CroppedAreaTopPixels>\n"
                "</rdf:Description>\n"
                "</rdf:RDF>\n"
                "</x:xmpmeta>";
        end = begin + strlen(begin) + 1;
    */

        // quick test2
        //  begin = "<tag>i love you <!-- comments -->, NOT!</tag>";
        //  end   = begin + strlen(begin);
        if (begin<end) {
            size_t const size = end - begin + 1; // plus a '\0'
            char* xml_head = new char[size];
            xml_tail_ = xml_head + size - 1;
            memcpy(xml_head, begin, size - 1);
            *xml_tail_ = '\0';
            xml_ptr1_  = xml_ptr2_ = xml_head;
            xml_element_stack_ = xml_attrib_stack_ = 0;

            // all set, ready to go...
            //BL_LOG("%s\n", xml_head);

            // error code :
            //   1 = end of xml
            //   0 = proceed(OK)
            //  -1 = bad xml structure
            //  -2 = user abort
            //  -3 = MAX_XML_ELEMENTES too small
            //  -4 = MAX_XML_ATTRIBUTES too small
            int error = 0;
            while (0==error) {
                error = ExtractXMLTag_();
                if (0==error) {
                    // to reach here :
                    //  1) xml_ptr1_ + 1 < xml_ptr2_,
                    //  2) *xml_ptr1_ = '<' and
                    //  3) *xml_ptr2_ = '>'

                    // skip '<', check if it a self-ended tag
                    error = ('/'==*(++xml_ptr1_)) ? ParseEndTag_():ParseStartTag_();
                }
            }
            delete[] xml_head;

            // check error code...
            return (1==error);
        }
        return false;
    }
};

//-----------------------------------------------------------------------------
// XMLWriter class
//-----------------------------------------------------------------------------
class File;
class XMLWriter
{
	enum { MAX_INDENT = 16, EXPAND_BUFFER_SIZE = 1024 };
	enum STATE {
		STATE_FAIL,
		STATE_CLOSED,
		STATE_OPEN,
		STATE_START_TAG,
		STATE_CONTENT,
		STATE_END_TAG,
	};

	// xml structures
	Array<uint32> xmlTag_;

	char*	stringBuffer_;
	uint32	stringBufferSize_;
	uint32	stringBufferPos_;	

	// file
	File*	filePtr_;

	STATE	state_;

	void	Flush_();
	uint32	PushString_(char const* str, uint32 leng);

public:
	explicit XMLWriter(char const* filename=NULL):
	xmlTag_(32),
	stringBuffer_(NULL),stringBufferSize_(0),stringBufferPos_(0),
	filePtr_(NULL),
	state_(STATE_CLOSED) {
		if (filename) {
			Open(filename);
		}
	}
	~XMLWriter() {
		Close();
		if (NULL!=stringBuffer_) {
			blFree(stringBuffer_);
			stringBuffer_ = NULL;
		}
		stringBufferSize_ = stringBufferPos_ = 0;
	}

	bool Fail() const { return (NULL==filePtr_);}
	bool Open(char const* xmlFile);
	void Close();

	// Start("tag1");
	//	Start("tag2"); End();
	// End();
	//
	// you have
	//
	// <tag1>
	//   <tag2 />
	// </tag1>
	//

	// return level of the tag, 0 for root, negative indicate error!
	int Start(char const* tag);
	int End();
	
	// attribute
	bool Attribute(char const* name, char const* value);

	// variety contents
	bool Content(char const* cont);
	bool Content(float const* float_array, int count);
	bool Content(int const* int_array, int count);
};

}}} // namespace nu::filesystem

#endif