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
 * @file	BLFile.h
 * @author	andre chen
 * @history	2012/01/19 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_FILE_H
#define BL_FILE_H

#include "BLCore.h"

namespace mlabs { namespace balai { namespace fileio {

// open mode
enum FILEOPEN {
	FOPEN_NA		= 0,
	FOPEN_READ		= 1,
	FOPEN_WRITE		= 2,
	FOPEN_READWRITE = FOPEN_READ|FOPEN_WRITE,
};

// mount device management
enum MOUNT_DEVICE {
	MOUNT_DISC,				// disc drive, CD-ROM, DVD-ROM, Blu-ray DVD...
	MOUNT_HDD0_OR_NAND,		// PS3 HDD0(Game Data), Wii(NAND), Win32/X360(HDD). save DISC to HDD in order to quick read
	MOUNT_HDD_SYSTEM_CACHE,	// PS3 HDD1 system cache
	MOUNT_SAVE_LOAD_GAME,	// Device where to save/load game
	MOUNT_APP_HOME,			// same location as executable file(host-PC home for ps3, same as MOUNT_DISC for Wii)
	MOUNT_NA,				// not a valid device
};

bool		mount(void*);
char const*	get_mount_dir();

// extract full path name
// cur_path [in] current_dir [out] full_path
// rel_file [in]
// max_path [in] buffer size of cur_path
// return total characters of out cur_path
uint32 extract_full_path(char* cur_path, char const* rel_file, uint32 max_path);
uint32 extract_full_path(char* full_path, char const* current_dir, char const* rel_filename, uint32 max_path);
inline uint32 get_full_path_name(char* full_path, char const* short_name, uint32 max_path) {
	return extract_full_path(full_path, get_mount_dir(), short_name, max_path);
}

// FileRead only for reading file, mostly optical disc file like CD-R, DVD, Blu-ray...
// these class files likely use large buffer for reading

struct AsyncRead {
	uint8* buffer;		// buffer for reading
	uint32 size;		// require size to read
	uint32 offset;		// file offset for reading
	void*  user_data;	// arbitrary user data
};
typedef void (*AsyncReadCallback)(AsyncRead* ar, uint32 error, uint32 size);

//-----------------------------------------------------------------------------
class FileRead
{
	//
	// make it zero size!
	//

protected:
	// ctor/dtor - use IFileRead::NewFile(uint32) and IFileRead::DeleteFile(IFileRead* file);
	FileRead() {}
	
	// don't call virtual function here! implementation class do the close!
	virtual ~FileRead() {}

public:
	// allocation info - return non-zero if this file is allocated using scratchpad 
	virtual uint32 AllocInfo() const = 0;

	// Open/close
	virtual bool Open(char const* filename)	= 0;
	virtual void Close() = 0;

	/* read - return actual bytes read */
	virtual uint32 Read(void* dest, uint32 bytes) = 0;

	/* read asynchronously - return error code(TBD) */
	virtual uint32 ReadAsync(AsyncRead* ar, AsyncReadCallback callback) = 0;
	
	// return position if found, else, BL_BAD_UINT32_VALUE. 
	virtual uint32 Seek(char c) = 0;
	
	// get current position, return ML_BAD_UINT32_VALUE if not OK
	virtual uint32 GetPosition() const = 0;

	// get file size, return 0 is file is closed.
	virtual uint32 GetSize() const = 0;

	// file seek
	virtual bool SeekSet(long offset) = 0;
	virtual bool SeekCur(long offset) = 0;
	virtual bool SeekEnd(long offset) = 0;
	
	// function template
	template<typename T>
	bool Read(T& t) { return sizeof(T)==Read(&t, sizeof(T)); }

	// factory method
	typedef FileRead file_type;
	static FileRead* New(uint32 allocOption=1);		// if not zero, use scratch pad
	static void Delete(FileRead* file);
};

//- File base class for general read/write file -------------------------------
class File
{
	virtual bool   OpenImp_(char const* filename, FILEOPEN openMode) = 0;
	virtual void   CloseImp_() = 0;
	virtual uint32 ReadImp_(void* pDst, uint32 bytes) = 0;
	virtual uint32 WriteImp_(void const* pSrc, uint32 bytes) = 0;
	virtual uint32 FillBytesImp_(char c, uint32 bytes) = 0;
	virtual bool   PaddingImp_(uint32 alignment) = 0;
	virtual uint32 SeekImp_(char c) = 0;

protected:
	FILEOPEN openMode_;
	
	// ctor/dtor - use File::NewFile(uint32) and File::DeleteFile(File*);
	File():openMode_(FOPEN_NA) {}
	
	// don't call virtual function here! implementation class do the close!
	virtual ~File() { BL_ASSERT(0==(FOPEN_READWRITE&openMode_)); }

public:
	// get current position, return BL_BAD_UINT32_VALUE if not OK
	virtual uint32 GetPosition() const = 0;

	// get file size 
	virtual uint32 GetSize() const = 0;

	// file seek
	virtual bool SeekSet(long offset) = 0;
	virtual bool SeekCur(long offset) = 0;
	virtual bool SeekEnd(long offset) = 0;

	// check open mode
	bool OpenMode(FILEOPEN mode) const { return 0!=(mode&openMode_); }
	bool IsOpen() const { return FOPEN_NA!=(FOPEN_READWRITE&openMode_); }

	// Open/close
	bool Open(char const* filename, FILEOPEN openMode=FOPEN_READ) {
		BL_ASSERT(filename && 0!=(FOPEN_READWRITE&openMode));
		
		// close if opened
		if (FOPEN_NA!=openMode_) {
			CloseImp_();
			openMode_ = FOPEN_NA;
		}

		// reopen
		if (!OpenImp_(filename, openMode))
			return false;

		openMode_ = openMode;
		return true;
	}
	void Close() {
		CloseImp_();
		openMode_ = FOPEN_NA;
	}

	bool EndOfFile() const {
		uint32 const size = GetSize();
		return (BL_BAD_UINT32_VALUE!=size) && (size==GetPosition());
	}

	/* read/write - return bytes read/write */
	uint32 Read(void* pDst, uint32 bytes) {
		if (0==(FOPEN_READ&openMode_) || NULL==pDst || bytes==0)
			return 0;
		return ReadImp_(pDst, bytes);
	}
	uint32 Write(void const* pSrc, uint32 bytes) {
		if (0==(FOPEN_WRITE&openMode_) || NULL==pSrc || bytes==0)
			return 0;
		return WriteImp_(pSrc, bytes);
	}
	uint32 Write(char const* cstr);
	uint32 FillBytes(char c, uint32 bytes) {
		if (0==(FOPEN_WRITE&openMode_) || bytes==0)
			return 0;
		return FillBytesImp_(c, bytes);
	}
	bool Padding(uint32 alignment) {
		if (0==(FOPEN_WRITE&openMode_) || alignment<2)
			return 0;
		return PaddingImp_(alignment);
	}

	// function template
	template<typename T>
	bool Read(T& t) {
		if (0==(FOPEN_READ&openMode_))
			return false;
		return sizeof(T)==ReadImp_(&t, sizeof(T));
	}
	template<typename T>
	bool Write(T const& t) {
		if (0==(FOPEN_WRITE&openMode_))
			return false;
		return sizeof(T)==WriteImp_(&t, sizeof(T));
	}

	// return position if found, else, ML_BAD_UINT32_VALUE. 
	uint32 Seek(char c) {
		if (FOPEN_READ!=openMode_)
			return BL_BAD_UINT32_VALUE;
		return SeekImp_(c);
	}

	// factory method
    typedef File file_type;
	static File* New(uint32 allocOption=0);	// if not zero, use scratch pad
	static void Delete(File* file);
};


// File utility - use me!
// File RAII - "Resource Acquisition Is Initialization"
class FILE_READ_RAII
{
	FileRead* file_;

public:	
	FILE_READ_RAII():file_(NULL) {}
	explicit FILE_READ_RAII(FileRead* file):file_(file) {}
	FILE_READ_RAII(char const* name, uint32 alloc):file_(NULL) {
		if (name) {
			file_ = FileRead::New(alloc);
			if (file_ && !file_->Open(name)) {
				FileRead::Delete(file_);
				file_ = NULL;
			}
		}
	}
	
	~FILE_READ_RAII() { 
		FileRead::Delete(file_);
		file_ = NULL;
	}
	operator FileRead*()   { return file_; }
	FileRead* operator->() { return file_; }
	bool fail() { return NULL==file_; }
	bool open(char const* name) {
		if (file_) {
			if (file_->Open(name))
				return true;

			// fail
			FileRead::Delete(file_);
			file_ = NULL;
			return false;
		}
		else {
			file_ = FileRead::New();
			if (NULL==file_)
				return false;
			
			if (!file_->Open(name)) {
				FileRead::Delete(file_);
				file_ = NULL;
				return false;
			}

			return true;
		}
	}
	FILE_READ_RAII& operator=(FILE_READ_RAII& rhs) {
		file_ = rhs.file_;
		rhs.file_ = NULL;
		return *this;
	}
};

class FILE_RAII
{
	File* file_;

public:	
	FILE_RAII():file_(NULL) {}
	explicit FILE_RAII(File* file):file_(file) {}
	FILE_RAII(char const* name, FILEOPEN mode, uint32 alloc=0):file_() {
		if (name) {
			file_ = File::New(alloc);
			if (file_ && !file_->Open(name, mode)) {
				File::Delete(file_);
				file_ = NULL;
			}
		}
	}
	
	~FILE_RAII() { 
		File::Delete(file_);
		file_ = NULL;
	}
	operator File*()   { return file_; }
	File* operator->() { return file_; }
	bool fail() { return NULL==file_ || !file_->IsOpen(); }
	bool open(char const* name, FILEOPEN mode) {
		if (file_) {
			if (file_->Open(name, mode))
				return true;

			// fail
			File::Delete(file_);
			file_ = NULL;
			return false;
		}
		else {
			file_ = File::New();
			if (NULL==file_)
			return false;
			
			if (!file_->Open(name, mode)) {
				File::Delete(file_);
				file_ = NULL;
				return false;
			}
			return true;
		}
	}
	FILE_RAII& operator=(FILE_RAII& rhs) {
		file_ = rhs.file_;
		rhs.file_ = NULL;
		return *this;
	}
};

}}}

#endif // BL_FILE_H