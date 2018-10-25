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
 * @file	BLMemory.h
 * @desc    memory management
 * @author	andre chen
 * @history	2011/12/28 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */

#ifndef BL_MEMORY_H
#define BL_MEMORY_H

#include "BLCore.h"
#include <cstring> // std::memcpy
#include <new>

#ifdef BL_DEBUG_BUILD
#ifndef BL_NO_MEMORY_TRACKER
#ifndef BL_TRACKING_MEMORY
#define BL_TRACKING_MEMORY
#endif
#endif
#endif

namespace mlabs { namespace balai { namespace system {

// memory allocate command
enum {
    MEM_CMD_MALLOC    = 0x00000000UL, // malloc, calloc, realloc
    MEM_CMD_NEW       = 0x00001000UL, // new
    MEM_CMD_ARRAY_NEW = 0x00002000UL, // new[]
    MEM_CMD_MASK      = 0x00003000UL, // command mask
};

//
// Memory pool - a contiguous chunk memory
//	1) can't be "static"(must live as long as IAllocator)
//  2) not intend to be inherited, use as "has-a"
//
class MemoryPool 
{
	// disable copy ctor and assignment operator
	BL_NO_COPY_ALLOW(MemoryPool);

	struct mhead* base_;	// base address of the mempool
	struct mhead* cap_;		// upper limit of the mempool
	struct mhead* used_;	// allocated memory chunk
	struct mhead* free_;	// free memory chunk

public:
	MemoryPool():base_(NULL),cap_(NULL),used_(NULL),free_(NULL) {}
	~MemoryPool() { reset(); }
	
	// Initialize
	void init(std::size_t beginAddress, std::size_t endAddress);

	// grow
	bool grow(void* buffer, std::size_t bufSize);

	// reset
	void reset() { base_ = cap_ = used_ = free_ = NULL; }

	// access by IAllocator
	struct mhead const* used() const { return used_; }
	struct mhead const* free() const { return free_; }

	// statistics
	bool   active() const { return NULL!=base_ && base_<cap_; }
	uint32 usedStats(uint32& chunk, uint32& inf, uint32& sup) const;
	uint32 freeStats(uint32& chunk, uint32& inf, uint32& sup) const;
	
	// coalesce free memory chunks
	uint32 compact();	// return the size of largest chunk

	// coalesce memory chunk mhdr. 
	// If the coalescent mhdr merge best, NULL returned, otherwise best returned.
	struct mhead* coalesce(struct mhead* mhdr, struct mhead* best);
	
	// find the header of ptr if the ptr comes from this mempool
	struct mhead* mheadchr(void* ptr) const;
	
	// malloc/free
	void* malloc(uint32 size, uint32 align=16, uint32 cmd=MEM_CMD_MALLOC);
	bool  free(void* ptr, uint32 cmd=MEM_CMD_MALLOC);
};


//
// IAllocator, singleton
//
class IAllocator
{
	BL_NO_COPY_ALLOW(IAllocator);

private:	
	MemoryPool main_; // main memory
	MemoryPool sub_; // aux memory used for short-term usage
	Mutex      mutex_;

	// initialization(probably call pre-main)
	virtual void OnInit_() = 0;

	// handler for memory exhausted
	virtual void OnOutOfMemory_(uint32 requested_size) = 0;
	
	// de-fragment, returns max memory chunk available
	uint32 Compact_();

	// malloc / realloc / free
	void* malloc_(std::size_t size, uint32 align, uint32 cmd, bool sub);
	void* realloc_(void* ptr, std::size_t size);
	bool  free_(void* ptr, uint32 cmd);

	uint32 DumpAllocMemory_(char const* mark, uint32 startID, uint32 endID);
	uint32 DumpFreeMemory_(char const* mark);
	
	// used memory statistics
	uint32 UsedStats_(uint32& chunks, uint32& inf, uint32& sup);
	uint32 FreeStats_(uint32& frags, uint32& inf, uint32& sup);

protected:
	IAllocator();
	virtual ~IAllocator(); // not necessary to be virtual, but some compilers will complain

	// called by derived class
	void InitMemPool_(void* mainHeap, size_t size, void* subHeap=NULL, size_t subSize=0);
    void FinishUp_(); // call by derived class's dtor

public:
	// unique instance. derived class must "IMPLEMENT_ALLOCATOR_CLASS"
	static IAllocator& GetInstance();

	// malloc / realloc / free
	static void* malloc(std::size_t size, uint32 align, uint32 cmd, bool sub) {
		return GetInstance().malloc_(size, align, cmd, sub);
	}
	static void* realloc(void* ptr, std::size_t size) {
		return GetInstance().realloc_(ptr, size);
	}
#ifdef BL_TRACKING_MEMORY
	static void* malloc(std::size_t size, uint32 align, uint32 cmd, bool sub, char const* file, int line);
	static void* realloc(void* ptr, std::size_t size, char const* file, int line);
#endif

	static bool free(void* ptr, uint32 cmd) {
		return (NULL==ptr)||GetInstance().free_(ptr, cmd);
	}

	// dump memory - parameters not used if release build
	static uint32 DumpAllocMemory(char const* mark=NULL, uint32 startID=0, uint32 endID=0xffffffff) {
		return GetInstance().DumpAllocMemory_(mark, startID, endID);
	}

	static uint32 DumpFreeMemory(char const* mark=NULL) {
		return GetInstance().DumpFreeMemory_(mark); 
	}

	// compact memory(defragment) - call this on idle, vertical retrace...
	static uint32 Compact() {
		return GetInstance().Compact_(); 
	}
	
	// statistics
	static uint32 UsedStats(uint32& chunks, uint32& inf, uint32& sup) {
		return GetInstance().UsedStats_(chunks, inf, sup);
	}

	static uint32 FreeStats(uint32& chunks, uint32& inf, uint32& sup) {
		return  GetInstance().FreeStats_(chunks, inf, sup);
	}
};

}}} // mlabs::balai::system

//-----------------------------------------------------------------------------
// The pointers returned by blMalloc may be released by calling blFree. 
// 16 bytes alignment is guaranteed by normal malloc calls, so you don't have to
// call blMallocAlignXXX with boundary 16 or less. Without boundary Specified,
// platform default alignment will be used(16 bytes alignment is still guaranteed)

#ifdef BL_TRACKING_MEMORY
// operator new/delete for memory tracking, to be coincident with normal operator
// new, these 2 operator new also throw std::bad_alloc.
// You may "#define new blNew" to activate memory tracking(if debug build)
// 
//#if defined(BL_OS_APPLE_iOS)
	// we don't overwrite global operator new/delete on iOS
	// #define blNew new
//#else

#define blNew new(__FILE__, __LINE__)
void* operator new(std::size_t, char const*, int) BL_EXCEPTION_SPEC_BADALLOC;
void* operator new[](std::size_t, char const*, int) BL_EXCEPTION_SPEC_BADALLOC;
void  operator delete(void*, char const*, int) BL_EXCEPTION_SPEC_NOTHROW;	/* in case ctor throw */
void  operator delete[](void*, char const*, int) BL_EXCEPTION_SPEC_NOTHROW; /* in case ctor throw */
//#endif

#define blMalloc(size)                 mlabs::balai::system::IAllocator::malloc(size, 0, mlabs::balai::system::MEM_CMD_MALLOC, false, __FILE__, __LINE__)
#define blMallocHigh(size)             mlabs::balai::system::IAllocator::malloc(size, 0, mlabs::balai::system::MEM_CMD_MALLOC, true, __FILE__, __LINE__)
#define blMallocAlign(size, align)     mlabs::balai::system::IAllocator::malloc(size, align, mlabs::balai::system::MEM_CMD_MALLOC, false, __FILE__, __LINE__)
#define blMallocAlignHigh(size, align) mlabs::balai::system::IAllocator::malloc(size, align, mlabs::balai::system::MEM_CMD_MALLOC, true, __FILE__, __LINE__)
#define blRealloc(ptr, size)           mlabs::balai::system::IAllocator::realloc(ptr, size, __FILE__, __LINE__)
#else
#define blNew new
#define blMalloc(size)                 mlabs::balai::system::IAllocator::malloc(size, 0, mlabs::balai::system::MEM_CMD_MALLOC, false)
#define blMallocHigh(size)             mlabs::balai::system::IAllocator::malloc(size, 0, mlabs::balai::system::MEM_CMD_MALLOC, true)
#define blMallocAlign(size, align)     mlabs::balai::system::IAllocator::malloc(size, align, mlabs::balai::system::MEM_CMD_MALLOC, false)
#define blMallocAlignHigh(size, align) mlabs::balai::system::IAllocator::malloc(size, align, mlabs::balai::system::MEM_CMD_MALLOC, true)
#define blRealloc(ptr, size)           mlabs::balai::system::IAllocator::realloc(ptr, size)
#endif
#define blFree(ptr)                    mlabs::balai::system::IAllocator::free(ptr, mlabs::balai::system::MEM_CMD_MALLOC)


// dump memory usage - 
// return     : total memory size in used
// Parameters : get startID and endID from GetAllocationID(),
//              debug build dump all memory in use between startID and endID
//              release build dump all memory in use
inline uint32 blDumpMemory(char const* mark=NULL, uint32 startID=0, uint32 endID=0xffffffff) {
	return mlabs::balai::system::IAllocator::DumpAllocMemory(mark, startID, endID);
}
inline uint32 blDumpFreeMemory() {
    return mlabs::balai::system::IAllocator::DumpFreeMemory();
}
// defragment memory - call this function if you free
inline void	blDefragmentMemory() {
    mlabs::balai::system::IAllocator::Compact();
}


//
// Memory check point
//
class MemoryCheckPoint
{
	uint32 allocID_;
	uint32 allocCount_;
	uint32 allocBytes_;
	uint32 highWatermark_;

public:
	// ctor (current memory states)
	MemoryCheckPoint();

	// reset to current memory states
	void Reset();

	// accessors
	uint32 AllocID() const { return allocID_; }
	uint32 AllocCount() const { return allocCount_; }
	uint32 AllocBytes() const { return allocBytes_; }
	uint32 HistoryHigh() const { return highWatermark_; }

	// comparsion
	bool operator==(MemoryCheckPoint const& rhs) const {
		return (allocCount_==rhs.allocCount_) && (allocBytes_==rhs.allocBytes_);
	}
	bool operator!=(MemoryCheckPoint const& rhs) const {
		return (allocCount_!=rhs.allocCount_) || (allocBytes_!=rhs.allocBytes_);
	}
};

// return leak memory in bytes
inline uint32 blCheckMemory(MemoryCheckPoint const& chkptA, char const* mark=NULL) {
	MemoryCheckPoint const chkptB;
	return (chkptB!=chkptA) ? blDumpMemory(mark, chkptA.AllocID(), chkptB.AllocID()):0;
}

namespace mlabs { namespace balai { namespace system {

//-----------------------------------------------------------------------------
// memory buffer - use has-a
//-----------------------------------------------------------------------------
class MemoryBuffer
{
    BL_NO_HEAP_ALLOC();

	uint8*	buffer_;
	uint32	bufSize_;
	uint32	writePos_;
	uint32	readPos_;
	uint32  alignment_;
	uint32  scratchPad_; // nonzero to indicate buffer is allocated from scratch pad

	// reserve buffer to write
	bool ReserveWriteBuffer_(uint32 bytes) {
		if ((writePos_+bytes)<=bufSize_) {
			BL_ASSERT(NULL!=buffer_);
			return true;
		}

		if (NULL==buffer_) {
			writePos_ = readPos_ = bufSize_ = 0;
			if (scratchPad_) {
				buffer_ = (uint8*) blMallocAlignHigh(bytes, alignment_);
				if (NULL!=buffer_) {
					bufSize_ = bytes;
					return true;
				}
				scratchPad_ = 0;
			}

			buffer_ = (uint8*) blMallocAlign(bytes, alignment_);
			if (NULL!=buffer_) {
				bufSize_ = bytes;
				return true;
			}
			return false;
		}

		BL_ASSERT(readPos_<=writePos_);

		// data saved
		uint32 const data_save = writePos_ - readPos_;
		bytes += data_save;
		if (bytes<=bufSize_) {
			if (data_save>0)
				std::memmove(buffer_, buffer_+readPos_, data_save);
			readPos_  = 0;
			writePos_ = data_save;

			return true;
		}

		// expand memory
		if (data_save==0) {
			writePos_ = readPos_  = 0;
			blFree(buffer_);
			if (scratchPad_) {
				buffer_ = (uint8*) blMallocAlignHigh(bytes, alignment_);
				if (buffer_) {
					bufSize_ = bytes;
					return true;
				}
			}
			else {
				buffer_ = NULL;
			}

			scratchPad_ = 0;
			buffer_ = (uint8*) blMallocAlign(bytes, alignment_);
			if (NULL==buffer_) {
				bufSize_ = 0;
				return false;
			}

			bufSize_  = bytes;
			return true;
		}
		
		// 
		uint8* ptr = NULL;
		uint32 usp = scratchPad_;
		if (usp) {
			ptr = (uint8*) blMallocAlignHigh(bytes, alignment_);
			if (NULL==ptr)
				usp = 0;
		}
		
		if (NULL==ptr) {
			ptr = (uint8*) blMallocAlign(bytes, alignment_);
			if (ptr==NULL) {
				return false;
			}
		}
		// copy memory
		std::memcpy(ptr, buffer_+readPos_, data_save);

		blFree(buffer_);
		buffer_     = ptr;
		bufSize_    = bytes;
		writePos_   = data_save;
		readPos_    = 0;
		scratchPad_ = usp;
		return true;
	}

	// disable copy ctor and assignment operator
	BL_NO_COPY_ALLOW(MemoryBuffer);

public:
	explicit MemoryBuffer(bool useScratchpad=true, uint32 alignment=0):
	  buffer_(NULL),bufSize_(0),writePos_(0),readPos_(0),
	  alignment_(alignment),scratchPad_(useScratchpad) {
		  if (0!=((alignment-1)&alignment))
		  	alignment_ = 0;
	}
	~MemoryBuffer() {
		if (buffer_) {
			blFree(buffer_);
			buffer_ = NULL;
		}
		bufSize_ = writePos_ = readPos_ = 0;
	}

	// rewind - discard all data write
	void   Reset()				 { readPos_ = writePos_ = 0; }
	uint8* GetBuffer() const	 { return buffer_;   }	
	uint32 GetWritePos() const	 { return writePos_; }
	uint32 GetReadPos() const	 { return readPos_;  }
	uint32 GetBufferSize() const { return bufSize_;  }
	uint32 GetDataSize() const   { return (readPos_<writePos_) ? (writePos_-readPos_):0; }

	// to write :
		// 1) get pointer to write
	uint8* GetWritePtr(uint32 bytesToWrite, uint32* maxbytesToWrite=NULL) {
		BL_ASSERT(bytesToWrite>0);
		if (!ReserveWriteBuffer_(bytesToWrite)) 
			return NULL;
		
		BL_ASSERT(writePos_+bytesToWrite <= bufSize_);	
		if (maxbytesToWrite)
			*maxbytesToWrite = (bufSize_ - writePos_);
		return (buffer_ + writePos_);
	}
		// 2) after write, move write pointer
	bool MoveWritePtr(uint32 offset) {
		offset += writePos_;
		if (offset>bufSize_)
			return false;
		writePos_ = offset;
		return true;
	}

	// to read
	uint32 Read(void* ptr, uint32 bytesToRead) {
		if (NULL==ptr || 0==bytesToRead || NULL==buffer_)
			return 0;

		BL_ASSERT(readPos_<=writePos_ && writePos_<=bufSize_);
		if (readPos_>=writePos_)
			return 0;

		if ((readPos_+bytesToRead)>writePos_)
			bytesToRead = (writePos_ - readPos_);

		std::memcpy(ptr, buffer_+readPos_, bytesToRead);
		readPos_ += bytesToRead;
		return bytesToRead;
	}

	// seek - move read position to specified character
	bool Seek(char c) {
		uint32 const rollback = readPos_;
		while (readPos_<writePos_) {
			BL_ASSERT(buffer_);
			if (buffer_[readPos_]==c)
				return true;
			++readPos_;
		}
		readPos_ = rollback;
		return false;
	}
};

}}}

// where should we go next...?
//
// Q) why not Hijack malloc/realloc/calloc/strdup/free ?
// A) No! since you can't mangle things under namespace std, including
//    malloc(), realloc(), calloc(), strdup() and free() ...
//    (This allocator is not intent to prevent you from using these functions)
//	  So, if you still use std::malloc/realloc/calloc/strdup..., be my guest,
//    but, use with care! (refer NuPS3Memory.cpp for some considerations)
// 
// Q) Use Phoenix singleton?
// A) The very first allocation object wake up our Allocator. that is, 
//    Allocator instantiates before any allocated objects. and automatically, 
//    it will die after them(c.f. ISO/IEC 14882 3.8 Object Lifetime).
//	  That being said, it works fine if we don't allocate memory post-main. 
//                                   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

#endif
