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
 * @file	BLStringPool.h
 * @desc    string pool
 * @author	andre chen
 * @history	2012/01/04 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_STRING_POOL_H
#define BL_STRING_POOL_H

#include "BLString.h"
#include "BLHashSet.h" // HashIndex, TArray<T>

namespace mlabs { namespace balai {

template<typename _traits>
class TStringPool
{
	class StringToken {
		TStringPool& pool_;
		uint32	crc_;
		uint32	start_;
		uint32	length_;

		// not define
		StringToken();
		StringToken& operator=(StringToken const&);
	
	public:
		StringToken(StringToken const& a):
			pool_(a.pool_),crc_(a.crc_),start_(a.start_),length_(a.length_) {}
		explicit StringToken(TStringPool& pool):
			pool_(pool),crc_(0),start_(0),length_(0) {}

		operator const char*() const { return pool_.Lookup(start_); }
		StringToken& operator=(char const* str) {
			if (NULL!=str)
				start_ = pool_.AddString(str, &crc_, &length_);
			else
				crc_ = start_ = length_ = 0;
				
			return *this;
		}
		bool operator==(StringToken const& rhs) const {
			if (length_!=rhs.length_)
				return false;

			if (&pool_==&(rhs.pool_)) {
				if (start_==rhs.start_) {
					BL_ASSERT(crc_==rhs.crc_);
					return true;
				}
				return false;
			}
			else {
				if (crc_!=rhs.crc_)
					return false;

				BL_ASSERT(0==std::memcmp(pool_.Lookup(start_), (char const*)rhs, length_));
				return true;
			}
		}
	};

	// constants
	enum { TEXT_TABLE_SIZE	  = 512,
		   STRING_BUFFER_GROW = 1024, 
		   TEXT_LIST_GROW	  = 128
	};

	struct Text {
		uint32	CRC;
		uint32	Start;
		uint32	Length;
		Text*	Next;
	};

	Text*        textTable_[TEXT_TABLE_SIZE];
	Array<Text*> allocBuffers_;
	Text*        freelist_;
	char*        stringBuf_;
	uint32       stringBufSize_;
	uint32       stringWritePos_;
	uint32       textCount_;

	Text* NewText_() {
		Text* text = freelist_;
		if (freelist_) {
			freelist_ = freelist_->Next;
			return text;
		}

		Text* alloc = blNew Text[TEXT_LIST_GROW]; // may throw
		for (uint32 i=1; i<TEXT_LIST_GROW; ++i) {
			alloc[i].Next = freelist_;
			freelist_ = alloc + i;
		}
		allocBuffers_.push_back(alloc);
		alloc->Next = NULL;
		return alloc;
	}

	// declare not define
	BL_NO_COPY_ALLOW(TStringPool);

public:
	TStringPool():allocBuffers_(8),freelist_(NULL),
	  stringBuf_(0),stringBufSize_(0),stringWritePos_(0),textCount_(0) {
		std::memset(textTable_, 0, TEXT_TABLE_SIZE*sizeof(Text*));
	}

	TStringPool(uint32 textCnt, uint32 stringBufSize):allocBuffers_(8),freelist_(NULL),
	  stringBuf_(0),stringBufSize_(0),stringWritePos_(0),textCount_(0) {
		if (textCnt>0) {
			if (textCnt<32)
				textCnt = 32;
			Text* alloc = blNew Text[textCnt]; // may throw
			for (uint32 i=0; i<textCnt; ++i) {
				alloc[i].Next = freelist_;
				freelist_ = alloc + i;
			}
			allocBuffers_.push_back(alloc);
		}
		if (stringBufSize>0) {
			stringBufSize = BL_ALIGN_UP(stringBufSize+1, 32);
			stringBuf_ = blNew char[stringBufSize];
			stringBufSize_ = stringBufSize;
			stringBuf_[0]   = '\0';
			stringWritePos_ = 1;
		}
		std::memset(textTable_, 0, TEXT_TABLE_SIZE*sizeof(Text*));
	}

	~TStringPool() { Clear(); }

	void Clear() {
		std::memset(textTable_, 0, TEXT_TABLE_SIZE*sizeof(Text*));
		while (!allocBuffers_.empty()) {
			Text* alloc = allocBuffers_.back();
			allocBuffers_.pop_back();
			delete[] alloc;
		}
		freelist_ = NULL;

		BL_SAFE_DELETE_ARRAY(stringBuf_);

		stringBufSize_ = stringWritePos_ = textCount_ = 0;
	}

	uint32 LoadFromRawData(char const* data, uint32 size, uint32 textCnt) {
		Clear();
		if (NULL==data || size<2)
			return 0;

		if (('\0'!=data[0]) || ('\0'!=data[size-1]))
			return 0;

		BL_TRY_BEGIN
			stringBufSize_ = BL_ALIGN_UP(size+1, 32);
			stringBuf_ = blNew char[stringBufSize_];
			stringBuf_[0]   = '\0';
			stringWritePos_ = 1;
			
			textCnt = BL_ALIGN_UP(textCnt+1, 32);
			Text* alloc = blNew Text[textCnt]; // may throw
			for (uint32 i=0; i<textCnt; ++i) {
				alloc[i].Next = freelist_;
				freelist_ = alloc + i;
			}
			allocBuffers_.push_back(alloc);
		BL_CATCH (std::bad_alloc const&)
			Clear();
			return 0;
		BL_CATCH_END

		char const* data_end = data + size;
		while (data<data_end) {
			if ('\0'==*data) {
				++data;
				continue;
			}

			uint32 len = 0;
			uint32 const crc = _traits::CRC(data, &len);
			std::size_t const packet = HashIndex(crc, TEXT_TABLE_SIZE);
			Text* hash = textTable_[packet];
			while (hash) {
				if (crc==hash->CRC && len==hash->Length)
					break; // duplicate

				hash = hash->Next;
			}

			if (NULL==hash) {
				// it's new...
				hash = NewText_(); // may throw

				hash->CRC    = crc;
				hash->Start  = stringWritePos_;
				hash->Length = len; 
				hash->Next   = textTable_[packet];
				textTable_[packet] = hash;

				// save string
				std::memcpy(stringBuf_+stringWritePos_, data, len);
				stringWritePos_ += len;
				stringBuf_[stringWritePos_++] = 0;
				++textCount_;
			}

			data += (len+1);
		}

		return textCount_;
	}

	uint32 GetTextCount() const			{ return textCount_; }
	uint32 GetStringLength() const		{ return stringWritePos_; }
	uint32 GetStringBufferSize() const	{ return stringBufSize_; }
	char const* GetStringBuffer() const { return stringBuf_; }

	bool Reserve(uint32 textExt, uint32 stringBufSizeExt) {
		if (textExt>0) {
			Text* text = freelist_;
			uint32 cap = 0;
			while (text && cap<textExt) {
				++cap;
				text = text->Next;
			}

			if (cap<textExt) {
				textExt -= cap;
				textExt = BL_ALIGN_UP(textExt, 4);
				BL_TRY_BEGIN
					Text* alloc = blNew Text[textExt]; // may throw
					for (uint32 i=0; i<textExt; ++i) {
						alloc[i].Next = freelist_;
						freelist_     = alloc + i;
					}
					allocBuffers_.push_back(alloc);
				BL_CATCH_ALL
					return false;
				BL_CATCH_END
			}
		}

		if ((stringBufSizeExt+=stringWritePos_)>=stringBufSize_) {
			BL_TRY_BEGIN
				stringBufSizeExt = BL_ALIGN_UP(stringBufSizeExt, 32);
				char* newStringPool_ = blNew char[stringBufSizeExt]; // may throw!
				if (NULL==stringBuf_) {
					newStringPool_[0] = '\0';
					stringWritePos_   = 1;
				}
				else {
					std::memcpy(newStringPool_, stringBuf_, stringWritePos_);
					delete[] stringBuf_;
				}
				stringBuf_     = newStringPool_;
				stringBufSize_ = stringBufSizeExt;
			BL_CATCH_ALL
				return false;
			BL_CATCH_END
		}

		return true;
	}

	uint32 AddString(char const* str, uint32* crcOut=NULL, uint32* lengOut=NULL) {
		if (NULL==str)
			return BL_BAD_UINT32_VALUE;

		if ('\0'==str[0]) {
			if (NULL==stringBuf_) {
				stringBuf_ = blNew char[STRING_BUFFER_GROW];
				stringBufSize_ = STRING_BUFFER_GROW;

				stringBuf_[0] = '\0';
				stringWritePos_ = 1;
			}
			BL_ASSERT('\0'==stringBuf_[0]);
			return 0;
		}

		uint32 len = 0;
		uint32 const crc = _traits::CRC(str, &len);
		std::size_t const packet = HashIndex(crc, TEXT_TABLE_SIZE);
		Text* hash = textTable_[packet];
		if (lengOut)
			*lengOut = len;
		while (hash) {
			if (crc==hash->CRC && len==hash->Length) {
				BL_ASSERT(_traits::CheckString(stringBuf_+hash->Start, str));
				if (crcOut)
					*crcOut = crc;
				return hash->Start;
			}
			hash = hash->Next;
		}

		// it's new...
		hash = NewText_(); // may throw

		if (crcOut)	
			*crcOut = crc;

		uint32 required_size = stringWritePos_ + len + 1;
		if (required_size>stringBufSize_) {
			required_size = BL_ALIGN_UP(required_size+1, STRING_BUFFER_GROW);

			char* newStringPool_ = blNew char[required_size]; // may throw!
			if (NULL==stringBuf_) {
				newStringPool_[0] = '\0';
				stringWritePos_ = 1;
			}
			else {
				std::memcpy(newStringPool_, stringBuf_, stringWritePos_);
				delete[] stringBuf_;
			}
			stringBuf_     = newStringPool_;
			stringBufSize_ = required_size;
		}

		// add text to hash table
		hash->CRC    = crc;
		hash->Start  = stringWritePos_;
		hash->Length = len; 
		hash->Next   = textTable_[packet];
		textTable_[packet] = hash;
		++textCount_;

		// save string
		std::memcpy(stringBuf_+stringWritePos_, str, len);
		stringWritePos_ += len;
		stringBuf_[stringWritePos_++] = 0;
		return hash->Start;
	}

	uint32 Find(char const* str) const {
		if (NULL==str)
			return BL_BAD_UINT32_VALUE;

		if ('\0'==str[0]) {
			if (NULL==stringBuf_)
				return BL_BAD_UINT32_VALUE;
			BL_ASSERT('\0'==stringBuf_[0]);
			return 0;
		}

		uint32 len = 0;
		uint32 const crc = _traits::CRC(str, &len);
		Text* hash = textTable_[HashIndex(crc, TEXT_TABLE_SIZE)];
		while (hash) {
			if (crc==hash->CRC && len==hash->Length) {
				BL_ASSERT(_traits::CheckString(stringBuf_+hash->Start, str));
				return hash->Start;
			}
			hash = hash->Next;
		}
		return BL_BAD_UINT32_VALUE;
	}

	// never trust char const*, i.e. you can't save char pointer for later using.
	// because the string buffer may realloc, move to somewhere...
	char const* Find(uint32 crc) const {
		Text* hash = textTable_[HashIndex(crc, TEXT_TABLE_SIZE)];
		while (hash) {
			if (crc==hash->CRC)
				return (stringBuf_+hash->Start);

			hash = hash->Next;
		}
		return NULL;
	}

	char const* Lookup(uint32 pos) const {
		if (pos>=stringWritePos_)
			return NULL;
//		BL_ASSERT(pos==0 || 0==stringBuf_[pos-1]);
		return (stringBuf_+pos);
	}

	//
	static uint32 HashString(char const* str) { return _traits::CRC(str, NULL); }
};


//-----------------------------------------------------------------------------
// case-sensitive string
struct CHAR_TRAITS {
	static uint32 CRC(char const* str, uint32* len) { 
		return CalcCRC(str, len); 
	}
	static bool CheckString(char const* str1, char const* str2) { 
		return (str1==str2) || CompareString(str1, str2); 
	}
};
typedef TStringPool<CHAR_TRAITS> StringPool;

//-----------------------------------------------------------------------------
// case-insensitive string
struct CI_CHAR_TRAITS {
	static uint32 CRC(char const* str, uint32* len) { 
		return CalcCICRC(str, len); 
	}
	static bool CheckString(char const* str1, char const* str2) { 
		return (str1==str2) || CompareStringCI(str1, str2); 
	}
};
typedef TStringPool<CI_CHAR_TRAITS> CIStringPool;

}} // namespace mlabs::balai

#endif
