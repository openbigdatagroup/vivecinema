/**
 *
 * HTC Corporation Proprietary Rights Acknowledgment
 * Copyright (c) 2011 HTC Corporation
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
 * @file	BLFileStream.h
 * @author	andre chen
 * @history	2012/01/30 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_FILE_STREAM_H
#define BL_FILE_STREAM_H

#include "BLCore.h"

namespace mlabs { namespace balai { namespace fileio {

//
// basic file input stream
//
template<typename file_traits>
class basic_ifstream 
{
    typename file_traits::file_type* file_;
    mutable uint32                   fails_;

public:
    basic_ifstream():file_(NULL),fails_(0) {}
    basic_ifstream(char const* name, uint32 alloc):file_(NULL),fails_(0) {
        if (name) {
            file_ = file_traits::New(alloc);
            if (file_ && !file_->Open(name)) {
                file_traits::Delete(file_);
                file_ = NULL;
                fails_ = 1;
            }
        }
        else {
            fails_ = 1;
        }
    }

	virtual ~basic_ifstream() { 
		file_traits::Delete(file_);
		file_  = NULL;
		fails_ = 0; 
	}
	uint32 Fail() const { return fails_; }
	bool Open(char const* filename) {
		fails_ = 0;
		if (file_) {
			if (file_->Open(filename))
				return true;

			// fail
			file_traits::Delete(file_);
			file_  = NULL;
			fails_ = 1;
			return false;
		}
		
		file_ = file_traits::New();
		if (NULL==file_) {
			fails_ = 1;
			return false;
		}

		if (!file_->Open(filename)) {
			file_traits::Delete(file_);
			file_ = NULL;
			fails_ = 1;
			return false;
		}

		return true;
	}

	uint32 GetPosition() const {
		uint32 pos = BL_BAD_UINT32_VALUE;
		if (NULL==file_ || BL_BAD_UINT32_VALUE==(pos=file_->GetPosition()))
			++fails_;
		return pos;
	}

	uint32 GetSize() const {
		uint32 size = 0;
		if (NULL==file_ || 0==(size=file_->GetSize()))
			++fails_;
		return size;
	}

	// seek
	bool SeekSet(long offset) {
		if (NULL==file_ || !file_->SeekSet(offset))
			++fails_;
		return (fails_==0);
	}
	bool SeekCur(long offset) {
		if (NULL==file_ || !file_->SeekCur(offset))
			++fails_;
		return (fails_==0);
	}
	bool SeekEnd(long offset) {
		if (NULL==file_ || !file_->SeekEnd(offset))
			++fails_;
		return (fails_==0);
	}

	// read unaligned bytes
	template<typename T>
	bool Read(T& t) { return (sizeof(T)==Read(&t, sizeof(T))); }
	uint32 Read(void* src, uint32 size) {
		uint32 read = 0;
		if (file_)
			read = file_->Read(src, size);
			
		if (0==read)
			++fails_;
		 
		return read;
	}

	// Arithmetic Extractors - refer [27.6.1.2.2]
	basic_ifstream& operator>>(int8& t) {
		if (NULL==file_ || !file_->Read(t))
			++fails_;
		return *this; 
	}
	basic_ifstream& operator>>(uint8& t) {
		if (NULL==file_ || !file_->Read(t))
			++fails_;
		return *this; 
	}
	basic_ifstream& operator>>(int16& t) {
		if (NULL==file_ || !file_->Read(t))
			++fails_;
		return *this; 
	}
	basic_ifstream& operator>>(uint16& t) {
		if (NULL==file_ || !file_->Read(t))
			++fails_;
		return *this; 
	}
	basic_ifstream& operator>>(int32& t) {
		if (NULL==file_ || !file_->Read(t))
			++fails_;
		return *this; 
	}
	basic_ifstream& operator>>(uint32& t) {
		if (NULL==file_ || !file_->Read(t))
			++fails_;
		return *this; 
	}
	basic_ifstream& operator>>(int64& t) {
		if (NULL==file_ || !file_->Read(t))
			++fails_;
		return *this; 
	}
	basic_ifstream& operator>>(uint64& t) {
		if (NULL==file_ || !file_->Read(t))
			++fails_;
		return *this; 
	}
	basic_ifstream& operator>>(float& f) {
		if (NULL==file_ || !file_->Read(f))
			++fails_;
		return *this; 
	}
	basic_ifstream& operator>>(double& d) {
		if (NULL==file_ || !file_->Read(d))
			++fails_;
		return *this; 
	}
};

}}} // namespace nu::filesystem


//
// Tool site code
//
#include "BLAssetPackage.h"
#include "BLFile.h"
#include "BLArray.h"

namespace mlabs { namespace balai { namespace fileio {

class ofstream
{
	struct ChunkPos {
		uint32	ID;
		uint32	Pos;
	};
	Array<ChunkPos> chunkStack_;
	String          fileName_;
	File*           file_;
	uint32          fail_;
	uint32 const    endianness_;

	// disable copy ctor and assignment operator
	BL_NO_COPY_ALLOW(ofstream);

	template<typename T> // T must be primitive type
	bool EndianWrite_(T t) {
		BL_COMPILE_ASSERT(is_primitive_type<T>::value, _ofstream_EndianWrite);
		BL_COMPILE_ASSERT(1!=sizeof(T), _ofstream_EndianWrite_1byte_data_not_comes_here);
		if (NULL==file_ || 0!=fail_)
			return false;

		if (0==endianness_) {
			if (!file_->Write(t))
				++fail_;
		}
		else {
			uint8 swapBuffer[sizeof(T)];
			uint8* src = (uint8*) &t;
			for (int i=sizeof(T); i>0; ++src)
				swapBuffer[--i] = *src;

			if (!file_->Write(swapBuffer, sizeof(T)))
				++fail_;
		}
		return (0==fail_);
	}

public:
	explicit ofstream(bool endianness=false):chunkStack_(),file_(NULL),fail_(0),endianness_(endianness) {}
	explicit ofstream(char const* filename, bool endianness=false):
		chunkStack_(),file_(NULL),fail_(0),endianness_(endianness) {
		if (filename) {
			file_ = File::New(1);
			if (NULL==file_) {
				fail_ = 1;
				return;
			}
			if (!file_->Open(filename, FOPEN_WRITE)) {
				File::Delete(file_);
				file_ = NULL;
				fail_ = 1;
				return;
			}

			fileName_ = filename;
		}
		else {
			fail_ = 1;
		}

		chunkStack_.reserve(32);
	}

	virtual ~ofstream() { 
		if (file_) {
			File::Delete(file_);
			file_ = NULL;
		}
	}
	char const* GetFileName() const {
		if (Fail())
			return NULL;
		return fileName_.c_str();
	}
	bool Fail() const { return (fail_!=0); }
	bool Open(char const* filename) {
		fail_ = 0;
		if (NULL==file_) {
			file_ = File::New();
			if (NULL==file_) {
				fail_ = 1;
				return false;
			}
		}

		if (!file_->Open(filename, FOPEN_WRITE)) {
			File::Delete(file_);
			file_ = NULL;
			fail_ = 1;
			return false;
		}

		fileName_ = filename;
		return true;
	}
	bool Close() {
		if (file_) {
			File::Delete(file_);
			file_ = NULL;
		}
		
		if (0!=fail_) {
			fail_ = 0;
			return false;
		}
		return true;
	}

	bool BeginFile(FileHeader& fhdr) {
		if (NULL==file_ || 0!=fail_ || !chunkStack_.empty())
			return false;

		if (0!=file_->GetPosition()) {
			fail_ = 1;
			return false;
		}

		// write file header	
		if (!EndianWrite_(fhdr.Platform)) {
			fail_ = 1;
			return false;
		}

		if (!EndianWrite_(fhdr.Version)) {
			fail_ = 1;
			return false;
		}

		if (offsetof(FileHeader, Size)!=file_->GetPosition()) {
			fail_ = 1;
			return false;
		}

		// size to be determined
		fhdr.Size = 0;
		EndianWrite_(fhdr.Size);
		WriteBytes(fhdr.Date, sizeof(fhdr.Date));
		WriteBytes(fhdr.Description, sizeof(fhdr.Description));

		if (sizeof(FileHeader)!=file_->GetPosition()) {
			fail_ = 1;
			return false;
		}

		return true;
	}

	uint32 EndFile() {
		if (NULL==file_ || 0!=fail_ || !chunkStack_.empty())
			return BL_BAD_UINT32_VALUE;

		uint32 const curPos = file_->GetPosition();
		if ((BL_BAD_UINT32_VALUE==curPos) || (curPos<sizeof(FileHeader)) || 
			(!file_->SeekSet(offsetof(FileHeader, Size)))) {
			fail_ = 1;
			return BL_BAD_UINT32_VALUE;
		}

		// fix size
		uint32 const size = (uint32)(curPos - sizeof(FileHeader));
		EndianWrite_(size);

		File::Delete(file_);
		file_ = NULL;
		if (0==fail_)
			return size;

		fail_ = 0;
		return BL_BAD_UINT32_VALUE;
	}

	bool BeginChunk(FileChunk const& chunk) {
		if (0==chunk.ID)
			return false;

		if (NULL==file_ || 0!=fail_)
			return false; // return (0==(++fail_));

		ChunkPos const cp = { chunk.ID, file_->GetPosition() };
		if (cp.Pos%32) {
			BL_LOG("[error] file %s with chunk 0x%X not 32B aligned\n", fileName_.c_str(), chunk.ID);
			fail_ = 1;
			return false;
		}
		EndianWrite_(chunk.ID);
		EndianWrite_(chunk.Version);
		EndianWrite_(chunk.Elements);
		BL_ASSERT((cp.Pos + offsetof(FileChunk, Size))==file_->GetPosition());
		EndianWrite_(chunk.Size);
		WriteBytes(chunk.Description, sizeof(chunk.Description));
		
		if (0==fail_) {
			chunkStack_.push_back(cp);
		    return true;
		}
		return false;
	}
	bool EndChunk(FileChunk& chunk) {
		if (NULL==file_ || 0!=fail_ || chunkStack_.empty())
			return false;

		ChunkPos const cp = chunkStack_.back();
		chunkStack_.pop_back();
		if (cp.ID!=chunk.ID) {
			fail_ = 1;
			return false;
		}

		uint32 curPos = file_->GetPosition();
		if (BL_BAD_UINT32_VALUE==curPos || (curPos<(cp.Pos+sizeof(FileChunk)))) {
			fail_ = 1;
			return false;
		}
		chunk.Size = (uint32)(curPos - cp.Pos - sizeof(FileChunk));
		
		// pad to 32B
		if (curPos%32) {
//			BL_LOG("fix padding - file %s with chunk 0x%X not 32B aligned\n", fileName_.c_str(), chunk.ID);
			uint32 const padup = BL_ALIGN_UP(curPos, 32);
			uint32 const pad = padup - curPos;
			BL_ASSERT(pad<32);
			char dummy[32] = { 0 };
			if (!WriteBytes(dummy, pad)) {
				fail_ = 1;
				return false;
			}
			curPos = padup;
		}

		if (!file_->SeekSet((long)(cp.Pos + offsetof(FileChunk, Elements)))) {
			fail_ = 1;
			return false;
		}

		// fixed!
//		EndianWrite_(chunk.ID);
//		EndianWrite_(chunk.Version);
		
		// #elements may change
		EndianWrite_(chunk.Elements);

		// fix size!
		EndianWrite_(chunk.Size);
//		WriteBytes(chunk.Description, sizeof(chunk.Description));

		if (!file_->SeekSet((long)curPos))
			++fail_;

		return (0==fail_);
	}

	// write bytes
	bool WriteBytes(void const* src, uint32 bytes) {
		if (NULL==file_ || NULL==src || !file_->Write(src, bytes)) 
			++fail_;
		return (0==fail_);
	}
	bool Padding(uint32 alignment) {
		if (NULL==file_ || !file_->Padding(alignment)) 
			++fail_;
		return (0==fail_);
	}
	bool SetPosition(uint32 pos) {
		if (NULL==file_ || !file_->SeekSet((long)pos))
			++fail_;
		return (0==fail_);
	}
	uint32 GetPosition() const {
		return (NULL==file_) ? BL_BAD_UINT32_VALUE:file_->GetPosition();
	}
	
	// Arithmetic Inserters - refer [27.6.2.5.2]
	ofstream& operator<<(int8 i) {
		if (NULL==file_ || !file_->Write(i)) 
			++fail_;
		return *this; 
	}
	ofstream& operator<<(uint8 i) {
		if (NULL==file_ || !file_->Write(i)) 
			++fail_;
		return *this;
	}

	ofstream& operator<<(int16 i) {
		BL_ASSERT(NULL!=file_);
		EndianWrite_(i);
		return *this; 
	}
	ofstream& operator<<(uint16 i) {
		BL_ASSERT(NULL!=file_);
		EndianWrite_(i);
		return *this; 
	}
	ofstream& operator<<(int32 i) {
		BL_ASSERT(NULL!=file_);
		EndianWrite_(i);
		return *this; 
	}
	ofstream& operator<<(uint32 i) {
		BL_ASSERT(NULL!=file_);
		EndianWrite_(i);
		return *this; 
	}
	ofstream& operator<<(float f) {
		BL_ASSERT(NULL!=file_);
		EndianWrite_(f);
		return *this; 
	}
	ofstream& operator<<(double d) {
		BL_ASSERT(NULL!=file_);
		EndianWrite_(d);
		return *this; 
	}
	// Add 2 inserters but drop 5s - "bool", "long", "unsigned long", "long double", "const void*"
	ofstream& operator<<(int64 i) {
		BL_ASSERT(NULL!=file_);
		EndianWrite_(i);
		return *this; 
	}
	ofstream& operator<<(uint64 i) {
		BL_ASSERT(NULL!=file_);
		EndianWrite_(i);
		return *this; 
	}
};

}}}

#endif
