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
 * @file	BLString.h
 * @desc    string class
 * @author	andre chen
 * @history	2012/01/04 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_STRING_H
#define BL_STRING_H

#include "BLCRC.h"

namespace mlabs { namespace balai {

//- string utilities ----------------------------------------------------------
inline uint32 StringToLowerCase(char* str)
{
	BL_ASSERT(str);
	uint32 len = 0;
	while (0!=str[len]) {
		if (64<str[len] && str[len]<91)
			str[len] = (char)(str[len] + 32);
		++len;
	}
	return len;
}
//-----------------------------------------------------------------------------
inline uint32 StringToUpperCase(char* str)
{
	BL_ASSERT(str);
	uint32 len = 0;
	while (0!=str[len]) {
		if (96<str[len] && str[len]<123)
			str[len] = (char)(str[len] - 32);
		++len;
	}
	return len;
}
//-----------------------------------------------------------------------------
inline uint32 StringLength(char const* str)
{
	BL_ASSERT(str);
	uint32 len = 0;
	while (0!=str[len])
		++len;

	return len;
}
//-----------------------------------------------------------------------------
inline uint32 StringCopy(char* dst, char const* src, uint32 limit)
{
	BL_ASSERT(src&&dst);
	for (uint32 len=0; len<limit; ++len) {
		if (0==(dst[len]=src[len]))
			return len;
	}
	dst[limit-1] = 0;
	return limit;
}
//-----------------------------------------------------------------------------
inline uint32 CompareString(char const* str1, char const* str2)
{
	BL_ASSERT(str1 && str2);
	uint32 len = 0;
	while (str1[len]==str2[len] && 0!=str1[len]) {
		++len;
	}

	return (0==str1[len] && 0==str2[len]) ? len:0;
}
//-----------------------------------------------------------------------------
inline uint32 CompareStringCI(char const* str1, char const* str2)
{
	BL_ASSERT(str1 && str2);
	uint32 len = 0;
	for (;;) {
		char a = str1[len];
		char b = str2[len];
		if (a!=b) {
			if (64<a && a<91)
				a = (char)(a + 32);
			else if (64<b && b<91)
				b = (char)(b + 32);

			if (a!=b)
				return 0;
		}
//      BL_ASSERT(a==b);
        if (0==a)
			return len;
		++len;
	}
}
//-----------------------------------------------------------------------------
inline uint32 ProcessFilename(char* dst, char const* src)
{
	BL_ASSERT(src && dst);
	// convert backslash('\') to slash('/', or stroke)
	char const slash  = '/';
	char const bslash = '\\';

	uint32 len = 0;
	while (src[len]) {
		if (bslash==src[len])
			dst[len] = slash;
		else
			dst[len] = src[len];

		++len;
	}
	return len;
}

// string class
class String
{
public:
	// constants
	enum { DEFAULT_SIZE			 = 32,	// thoughts : each allocation have at least 16 bytes overhead
		   MIN_CAPACITY			 = DEFAULT_SIZE - sizeof(char*) - 2*sizeof(uint16),
		   // flag
		   FLAG_CASE_INSENSITIVE = 0x0001,
		   FLAG_STRING_DIRTY	 = 0x0002,
		   FLAG_CAPACITY_MASK    = 0xFFFC,
		   // length
		   SHORT_LENGTH			 = MIN_CAPACITY - 1,
		   MAX_LENGTH			 = FLAG_CAPACITY_MASK - 1 }; 
private:
	// Text is stored as null-terminated character string in memory.  The
	// length counts all but the non-null character.  When written to disk,
	// the length is stored first and all but the non-null character are
	// stored second.
	char*	c_str_long_;
	mutable uint16 length_;
	uint16	flags_;
	char	c_str_short_[MIN_CAPACITY];

public:
    String(char const* acText=NULL, bool caseInsensitive=false);
    String(String const&);
	explicit String(uint16 capacity);
	~String() { Clear(); }

	// clear
	void Clear();

	// truncate substring
	String const SubStr(int p0, int n0);

    // member access
	int Capacity() const { return (flags_&FLAG_CAPACITY_MASK); }
	void CaseInsensitive() { flags_ |= FLAG_CASE_INSENSITIVE; }
	char const* c_str() const { return (NULL==c_str_long_) ? c_str_short_:c_str_long_; }
	char operator[](int id) const {
		BL_ASSERT(id<length_);
		if (NULL==c_str_long_) {
			BL_ASSERT(id<MIN_CAPACITY);
			return c_str_short_[id];
		}
		else {
			return c_str_long_[id];
		}
	}
	char& operator[](int id) { 
		BL_ASSERT(id<length_);
		flags_ |= FLAG_STRING_DIRTY;
		if (NULL==c_str_long_) {
			BL_ASSERT(id<MIN_CAPACITY);
			return c_str_short_[id];
		}
		else {
			return c_str_long_[id];
		}
	}

	uint16 Length() const {
		if (flags_&FLAG_STRING_DIRTY) {
			if (NULL!=c_str_long_) {
				uint32 const leng = StringLength(c_str_long_);
				BL_ASSERT(leng<=MAX_LENGTH);
				length_ = (uint16) leng;
			}
			else {
				uint32 const leng = StringLength(c_str_short_);
				BL_ASSERT(leng<MIN_CAPACITY);
				length_ = (uint16) leng;
			}
		}
		return length_; 
	}

#if 0
	// Hashing Function of ELF - http://x86.ddj.com/ftp/manuals/tools/elf.pdf
	// (currently not use)
	uint32 Hash() const {
		uint32 h = 0, g;
		char const* name = (NULL==c_str_long_) ? c_str_short_:c_str_long_;
		while (*name) {
			h = (h << 4) + *name++;
			g = (h & 0xf0000000);
			if (0!=g)
				h ^= g >> 24;
			h &= ~g;
		}
		return h;
	}
#endif

    // assignment, comparisons, implicit conversion
    String& operator=(String const& s);
    String& operator+=(String const& s);
    bool operator==(String const& s) const;
	bool operator==(char const* s) const;
	bool operator!=(String const& s) const { return !(*this==s); }
	bool operator!=(char const* s) const { return !(*this==s); }
	operator uint32() const {
		return (flags_&FLAG_CASE_INSENSITIVE) ? CalcCICRC(c_str()):CalcCRC(c_str());
	}

    // bad idea?!
    operator char const*() const { return c_str(); }

	// Cases
    String const& ToUpper();
    String const& ToLower();

	// call if dirty
	void Refresh();

	// switch
	void Swap(String& str);
};

//BL_COMPILE_ASSERT(24==String::MIN_CAPACITY, String_min_capacity_not_OK);
BL_COMPILE_ASSERT(String::DEFAULT_SIZE==sizeof(String), sizeof_String_not_OK);

//----------------------------------------------------------------------------
inline String const operator+(String a, String const& b) { return a+=b; }


}} // namespace mlabs::balai

#endif