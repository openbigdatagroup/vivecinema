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
 * @file	BLCtdFile.h
 * @desc    C standard file io implementation
 * @author	andre chen
 * @history	2012/01/19 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#include "BLFile.h"
#include "BLMemory.h"
#include <errno.h>

namespace mlabs { namespace balai { namespace fileio {

//
// using C standard file api
//
template<uint32 read_cache_size, uint32 class_desc_4cc>
class CStdFileRead : public FileRead
{
	enum {
		READ_CACHE_SIZE     = BL_ALIGN_UP(read_cache_size, 1024),
		READ_CACHE_HALFSIZE = READ_CACHE_SIZE>>1,
		CLASS_EXTRA_INFO	= class_desc_4cc,
	};	
	char	buffer_[READ_CACHE_SIZE];
	FILE*	file_;
	uint32	fileSize_;
	uint32	filePos_;
	uint32	bufPos_;
	uint32	bufRead_;
	uint32 const extraInfo_;

	// declare but no define
	BL_NO_COPY_ALLOW(CStdFileRead);
	
protected:	
	explicit CStdFileRead(uint32 extraInfo=0):FileRead(),
      file_(NULL),fileSize_(0),filePos_(0),bufPos_(0),bufRead_(0),
	  extraInfo_(extraInfo==0 ? 0:CLASS_EXTRA_INFO){}
	~CStdFileRead() { Close(); }

public:
	uint32 AllocInfo() const { return extraInfo_; }
	bool Open(char const* filename) {
		if (NULL!=file_)
			Close();
	
		fileSize_ = filePos_ = bufPos_ = bufRead_ = 0;
		file_ = NULL;
		if (NULL==filename)
			return false;

		fileSize_ = get_full_path_name(buffer_, filename, READ_CACHE_SIZE);
		BL_ASSERT(fileSize_<READ_CACHE_SIZE);
		fileSize_ = 0;

		// Note : We don't use text(translated) mode!
		file_ = fopen(buffer_, "rb");
		if (NULL==file_) {
			BL_LOG("error open file %s:%s\n", buffer_, strerror(errno));
			return false;
		}
		
		// file size
		fseek(file_, 0, SEEK_END);
		fileSize_ = ftell(file_);
		rewind(file_); // fseek(file_, 0, SEEK_SET);

		return true;
	}
	void Close() {
		fileSize_ = filePos_ = bufPos_ = bufRead_ = 0;
		if (NULL!=file_) {
			fclose(file_);
			file_ = NULL;
		}
	}
	uint32 Read(void* pDst, uint32 bytes) {
		if (NULL==file_ || NULL==pDst)
			return 0;

		uint32 const curPos   = filePos_ + bufPos_; // current position
		uint32 const max_read = (curPos<fileSize_) ? (fileSize_ - curPos):0;
		if (max_read<bytes)
			bytes = max_read;
		
		if (0==bytes)
			return 0;

		if ((bufPos_+bytes)<=bufRead_) {
			std::memcpy(pDst, (buffer_+bufPos_), (std::size_t) bytes);
			bufPos_ += bytes;
			return bytes;
		}

		uint32 done = 0;
		if (bufRead_>bufPos_) {
			done = bufRead_ - bufPos_;
			std::memcpy(pDst, (buffer_+bufPos_), (std::size_t)done);
			pDst  = ((char*)pDst) + done;
			bytes -= done;
		}
		filePos_ += bufRead_;
		bufRead_ = bufPos_ = 0;
		BL_ASSERT(0!=bytes);

		if (bytes>READ_CACHE_HALFSIZE) {
			bytes = (uint32)fread(pDst, 1, bytes, file_);
			filePos_ += bytes;
			return done+bytes;
		}

		bufRead_ = (uint32) fread(buffer_, 1, READ_CACHE_SIZE, file_);
		if (bufRead_==0)
			return done;

		if (bufRead_>bytes) {
			std::memcpy(pDst, buffer_, (std::size_t)bytes);
			bufPos_ = bytes;
		}
		else {
			std::memcpy(pDst, buffer_, (std::size_t)bufRead_);
			bytes = bufRead_;
			filePos_ += bytes;
			bufRead_ = 0;
		}
		
		return (done+bytes);
	}
	// -pending- ///////////////////////////////////////////////////////////////////
	uint32 ReadAsync(AsyncRead* /*ar*/, AsyncReadCallback /*callback*/) {
		/* pending */
		BL_HALT("CStdFileRead::ReadAsync() not implement!");
		return 0;
	}
	////////////////////////////////////////////////////////////////////////////////
	uint32 Seek(char c) {
		if (NULL==file_)
			return BL_BAD_UINT32_VALUE;

		uint32 const rollback = filePos_ + bufPos_;
		while (bufPos_<bufRead_) {
			if (c==buffer_[bufPos_])
				return filePos_ + bufPos_;
			++bufPos_;
		}

		filePos_ += bufRead_;
		for (;;) {
			bufRead_ = (uint32) fread(buffer_, 1, READ_CACHE_SIZE, file_);

			for (bufPos_=0; bufPos_<bufRead_; ++bufPos_) {
				if (c==buffer_[bufPos_])
					return filePos_ + bufPos_;
			}
			
			filePos_ += bufRead_;
			if (bufRead_!=READ_CACHE_SIZE)
				break;
			
			bufRead_ = bufPos_ = 0;
		}

		bufRead_ = bufPos_ = 0;
		filePos_ = rollback;
		fseek(file_, rollback, SEEK_SET);
		return BL_BAD_UINT32_VALUE;
	}
	bool SeekSet(long offset) {
		if (NULL==file_)
			return false;
		
		if (offset<=0) {
			if (0==filePos_) {
				bufPos_ = 0;
				return true;
			}
			offset = 0;
		}
		else if (fileSize_<=(uint32)offset) {
			bufPos_ = bufRead_ = 0;
			filePos_ = fileSize_;
			return (0==fseek(file_, 0, SEEK_END));
		}
		else if (filePos_ <= (uint32)offset && (uint32)offset<=(filePos_+bufRead_)) {
			bufPos_ = (offset - filePos_);
			return true;
		}
		
		bufPos_ = bufRead_ = 0;
		filePos_ = offset;
		return (0==fseek(file_, filePos_, SEEK_SET));
	}
	bool SeekCur(long offset) {
		if (NULL==file_)
			return false;

		long rel = (long)bufPos_ + offset;
		if (0<=rel && rel<=(long)bufRead_) {
			bufPos_ = (long) rel;
			return true;
		}

		rel += (long) filePos_;
		if (rel<=0) {
			if (0==filePos_) {
				bufPos_ = 0;
				return true;
			}
			rel = 0;
		}
		else if (rel>=(long)fileSize_) {
			bufPos_ = bufRead_ = 0;
			filePos_ = fileSize_;
			return (0==fseek(file_, 0, SEEK_END));
		}

		bufPos_ = bufRead_ = 0;
		filePos_ = rel;
		return (0==fseek(file_, filePos_, SEEK_SET));
	}

    // C++11 implementations are allowed to not meaningfully support SEEK_END
    // (therefore, code using it has no real standard portability)
	bool SeekEnd(long offset) {
		if (NULL==file_)
			return false;

		if (offset<=0) {
			filePos_ = fileSize_;
			bufRead_ = bufPos_ = 0;
			return (0==fseek(file_, 0, SEEK_END));
		}

		if ((uint32)offset>=fileSize_) {
			bufPos_ = 0;
			if (0==filePos_)
				return true;

			bufRead_ = filePos_ = 0;
			return (0==fseek(file_, filePos_, SEEK_SET));
		}

		uint32 rel = fileSize_ - offset;
		if (filePos_<=rel && rel<=(filePos_+bufRead_)) {
			bufPos_ = (uint32) (rel - filePos_);
			return true;
		}

		bufPos_  = bufRead_ = 0;
		filePos_ = rel;
		return (0==fseek(file_, filePos_, SEEK_SET));
	}

	// get current position
	uint32 GetPosition() const {
		if (NULL==file_)
			return BL_BAD_UINT32_VALUE;

		BL_ASSERT(ftell(file_)==(long)(filePos_+bufRead_));
		return (filePos_+bufPos_);
	}
	uint32 GetSize() const { return fileSize_; }
	
	// grant access to IFileRead::NewFile
	friend class FileRead;
};

//
// using C standard file api
//
template<uint32 read_cache_size>
class CStdFile : public File
{
	// write/read buffer
	// NOTE : FILE has its own buffer(probably 4K for win32), but we still benefit from
	//        preventing mass fread/fwrite calls. Hope this worths it.
	enum { 
		BUFFER_SIZE		 = BL_ALIGN_UP(read_cache_size, 1024),
		BUFFER_HALF_SIZE = (BUFFER_SIZE>>1)
	};
	char	buffer_[BUFFER_SIZE];
	FILE*	file_;
	uint32  bufPos_;
	uint32  bufRead_;
	uint32	fileSize_;

	//-------------------------------------------------------------------------
	bool OpenImp_(char const* filename, FILEOPEN openMode) {
		BL_ASSERT(NULL==file_);
		bufPos_ = bufRead_ = fileSize_ = 0;

		// get full path...
		fileSize_ = get_full_path_name(buffer_, filename, BUFFER_SIZE);
		BL_ASSERT(fileSize_<BUFFER_SIZE);
		fileSize_ = 0;

		// Note : We don't use text(translated) mode!
		switch (openMode)
		{
		case FOPEN_READ:
			// Opens for reading. If the file does not exist or cannot be found, the fopen call fails.
			file_ = fopen(buffer_, "rb"); 
			if (NULL!=file_) {
				fseek(file_, 0, SEEK_END);
				fileSize_ = ftell(file_);
				rewind(file_); // fseek(file_, 0, SEEK_SET);
			}
			break;
		case FOPEN_WRITE:
			// Opens an empty file for writing. If the given file exists, its contents are destroyed.
			file_ = fopen(buffer_, "wb");
			break;
		case FOPEN_READWRITE:
			// Opens an empty file for both reading and writing. If the given file exists, its contents are destroyed.
			file_ = fopen(buffer_, "w+b");
			break;
		default:
			return false;
		}

		return (NULL!=file_);
	}
	//-------------------------------------------------------------------------
	void CloseImp_() {
		if (NULL==file_) {
			bufPos_ = bufRead_ = fileSize_ = 0;
			return;
		}

		if (FOPEN_WRITE==openMode_ && bufPos_>0)
			fwrite(buffer_, 1, (std::size_t)bufPos_, file_);

		fclose(file_);
		file_ = NULL;
		bufPos_ = bufRead_ = fileSize_ = 0;
	}
	//-------------------------------------------------------------------------
	uint32 ReadImp_(void* pDst, uint32 bytes) {
		BL_ASSERT(file_ && (FOPEN_READ==openMode_||FOPEN_READWRITE==openMode_) );
		if (FOPEN_READWRITE==openMode_)
			return (uint32) fread(pDst, 1, (std::size_t)bytes, file_);

		// openMode_ = FOPEN_READ
		if ((bufPos_+bytes)<=bufRead_) {
			std::memcpy(pDst, (buffer_+bufPos_), (std::size_t) bytes);
			bufPos_ += bytes;
			return bytes;
		}

		uint32 done = 0;
		if (bufRead_>bufPos_) {
			done = bufRead_ - bufPos_;
			std::memcpy(pDst, (buffer_+bufPos_), (std::size_t)done);
			pDst  = ((char*)pDst) + done;
			bytes -= done;
		}
		bufRead_ = bufPos_ = 0;
		BL_ASSERT(0!=bytes);

		if (bytes>BUFFER_HALF_SIZE)
			return done + (uint32)fread(pDst, 1, bytes, file_);

		bufRead_ = (uint32) fread(buffer_, 1, BUFFER_SIZE, file_);
		if (bufRead_>bytes) {
			std::memcpy(pDst, buffer_, (std::size_t)bytes);
			bufPos_ = bytes;
			return (done+bytes);
		}
		else if (bufRead_>0) {
			std::memcpy(pDst, buffer_, (std::size_t)bufRead_);
			bytes = bufRead_; 
			bufRead_ = 0;
			return (done+bytes);
		}
		else
			return done;
	}
	//-------------------------------------------------------------------------
	uint32 SeekImp_(char c) {
		BL_ASSERT(FOPEN_READ==openMode_);
		if (NULL==file_)
			return BL_BAD_UINT32_VALUE;
		
		uint32 curPos = (uint32) ftell(file_);
		uint32 const rollback = curPos + bufPos_ - bufRead_; 
		while (bufPos_<bufRead_) {
			if (c==buffer_[bufPos_])
				return (curPos + bufPos_ - bufRead_);
			++bufPos_;
		}

		bufRead_ = BUFFER_SIZE;
		while (bufRead_== BUFFER_SIZE) {
			bufRead_ = (uint32) fread(buffer_, 1, BUFFER_SIZE, file_);
			
			for (bufPos_=0; bufPos_<bufRead_; ++bufPos_) {
				if (c==buffer_[bufPos_])
					return (curPos + bufPos_);
			}

			curPos += bufRead_;
		}

		fseek(file_, rollback, SEEK_SET);
		bufPos_ = bufRead_ = 0;
		return BL_BAD_UINT32_VALUE;
	}
	//-------------------------------------------------------------------------
	uint32 WriteImp_(void const* pSrc, uint32 bytes) {
		BL_ASSERT(file_ && (FOPEN_WRITE==openMode_||FOPEN_READWRITE==openMode_) );
		if (FOPEN_READWRITE==openMode_)
			return (uint32)fwrite(pSrc, 1, (std::size_t)bytes, file_);

		// openMode_ == FOPEN_WRITE
		BL_ASSERT(bufPos_<=BUFFER_SIZE);

		if ((bufPos_+bytes)<=BUFFER_SIZE) {
			std::memcpy((buffer_+bufPos_), pSrc, (std::size_t) bytes);
			bufPos_ += bytes;
			return bytes;
		}

		// flush
		if (bufPos_>0) {
			if (bufPos_!=(uint32) fwrite(buffer_, 1, bufPos_, file_)) {
				bufPos_ = 0;
				return 0; // fail!
			}
			bufPos_ = 0;
		}

		// write to file
		if (bytes>=BUFFER_SIZE) 
			return (uint32) fwrite(pSrc, 1, bytes, file_);
		
		// write to buffer
		std::memcpy(buffer_, pSrc, (std::size_t) bytes);
		bufPos_ = bytes;
		return bytes;
	}
	//-------------------------------------------------------------------------
	uint32 FillBytesImp_(char c, uint32 bytes) {
		BL_ASSERT(file_ && (0<bytes) && (FOPEN_WRITE&openMode_));

		uint32 write = 0;
		uint32 tmp;
		if (FOPEN_WRITE==openMode_) {
			if ((bufPos_+bytes)<=BUFFER_SIZE) {
				std::memset((buffer_+bufPos_), c, bytes);
				bufPos_ += bytes;
				return bytes;
			}
		
			BL_ASSERT(bufPos_<=BUFFER_SIZE);
			write = BUFFER_SIZE - bufPos_;
			if (write>0)
				std::memset((buffer_+bufPos_), c, write);

			tmp = (uint32)fwrite(buffer_, 1, BUFFER_SIZE, file_);
			if (tmp!=BUFFER_SIZE) {
				if (tmp>bufPos_) {
					tmp -= bufPos_;
					bufPos_ = 0;
					return tmp;
				}	
				else {
					bufPos_ = 0;
					return 0;
				}
			}	
			bufPos_ = 0;
			bytes -= write;
		}
		if (0==bytes)
			return write;
		
		std::memset(buffer_, c, BUFFER_SIZE);
		while (bytes>=BUFFER_SIZE) {
			tmp = (uint32)fwrite(buffer_, 1, BUFFER_SIZE, file_);
			write += tmp;
			if (tmp!=BUFFER_SIZE)
				return write;

			bytes -= BUFFER_SIZE;
		}

		if (FOPEN_WRITE==openMode_) {
			bufPos_ = bytes;
			return (write+bufPos_);
		}

		// FOPEN_READWRITE==openMode_
		if (bytes==0)
			return write;

		return write + (uint32)fwrite(buffer_, 1, (std::size_t)bytes, file_);
	}
	//-------------------------------------------------------------------------
	bool PaddingImp_(uint32 alignment) {
		BL_ASSERT(file_ && (FOPEN_WRITE==openMode_||FOPEN_READWRITE==openMode_));
		uint32 const alignMask = alignment - 1;
		BL_ASSERT(0==(alignMask&alignment)); // must be power of 2
		
		uint32 curPos = (uint32) ftell(file_);
		if (FOPEN_WRITE==openMode_)
			curPos += bufPos_;
		
		uint32 const padding = ((curPos + alignMask) & (~alignMask)) - curPos;
		if (0==padding)
			return true;

		return (padding==FillBytesImp_(0, padding));
	}
	
	// ctor/dtor
	CStdFile():File(),file_(NULL),bufPos_(0),bufRead_(0),fileSize_(0) {}
	~CStdFile() { Close(); }

public:
	// only for read mode
	uint32	GetSize() const {
		if (NULL==file_)
			return 0;

		if (FOPEN_READ==openMode_)
			return fileSize_;
		else
			return BL_BAD_UINT32_VALUE;
	}

	// get current position
	uint32 GetPosition() const {
		if (NULL==file_)
			return BL_BAD_UINT32_VALUE;

		uint32 const curPos = (uint32) ftell(file_);
		if (FOPEN_WRITE==openMode_)
			return (curPos + bufPos_);
		else if (FOPEN_READ==openMode_ && bufPos_<bufRead_) {
			BL_ASSERT(curPos>=bufRead_);
			return (curPos + bufPos_ - bufRead_);
		}

		return curPos;
	}

	// seek
	bool SeekSet(long offset) {
		if (NULL==file_)
		return false;

		if (FOPEN_READ==openMode_) {
			bufRead_ = 0;
		}
		else if (FOPEN_WRITE==openMode_ && bufPos_>0) {
			if (bufPos_!=fwrite(buffer_, 1, (std::size_t)bufPos_, file_)) {
				bufPos_ = 0;
				return false;
			}
		}
		bufPos_ = 0;
		return (0==fseek(file_, offset, SEEK_SET));
	}
	
	bool SeekCur(long offset) {
		if (NULL==file_)
			return false;
		
		if (offset==0)
			return true;

		if (FOPEN_READ==openMode_) {
			BL_ASSERT(bufRead_>=bufPos_ && bufPos_>=0);
			long tmp = (long)bufPos_ + offset;
			if (offset<0) {
				if (tmp>=0) {
					bufPos_ = (uint32) tmp;
					return true;
				}
			}
			else {
				if (tmp<=(long)bufRead_) {
					bufPos_ = (uint32) tmp;
					return true;
				}
			}

			offset = offset + (long)bufPos_ - (long)bufRead_;
			bufPos_ = bufRead_ = 0;
		}
		else if (FOPEN_WRITE==openMode_ && bufPos_>0) {
			if (bufPos_!=(uint32)(fwrite(buffer_, 1, (std::size_t)bufPos_, file_))) {
				bufPos_ = 0;
				return false;
			}
			bufPos_ = 0;
		}

		return (0==fseek(file_, offset, SEEK_CUR));
	}
	bool SeekEnd(long offset) {
		if (NULL==file_)
			return false;

		if (FOPEN_READ==openMode_) {
			bufPos_ = bufRead_ = 0;
		}
		else if (FOPEN_WRITE==openMode_ && bufPos_>0) {
			fwrite(buffer_, 1, (std::size_t)bufPos_, file_);
			bufPos_ = 0;
		}

		return (0==fseek(file_, offset, SEEK_END));
	}

	// grant ctor/dtor access
	friend class File;
};

}}}
