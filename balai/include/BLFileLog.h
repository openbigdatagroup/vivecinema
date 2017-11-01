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
 * @file	BLFileLog.h
 * @desc    file log
 * @author	andre chen
 * @history	2012/01/31 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_FILE_LOG_H
#define BL_FILE_LOG_H

#include "BLCore.h"

namespace mlabs { namespace balai { namespace fileio {

class File;

class FileLog
{
	File* file_;
	uint32 infos_;
	uint32 warnings_;
	uint32 errors_;
	BL_NO_COPY_ALLOW(FileLog);

public:
	FileLog():file_(NULL),infos_(0),warnings_(0),errors_(0) {}
	~FileLog() { End(); }
	uint32 Errors() const { return errors_; }
	bool Start(char const* filename, char const* title);
	void End();

	uint32 Info(char const* format, ...);
	uint32 Warning(char const* format, ...);
	uint32 Error(char const* format, ...);
};

}}}

#endif // BL_FILE_LOG_H