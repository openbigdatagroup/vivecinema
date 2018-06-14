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
 * @file	BLStreamFWD.h
 * @author	andre chen
 * @desc    include this file when fileio::ifstream declaration is needed.
 *          if definition is needed, you include BLFileStream.h instead
 * @history	2012/01/30 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_STREAM_FORWARD_DECLARATIONS_H
#define BL_STREAM_FORWARD_DECLARATIONS_H

namespace mlabs { namespace balai { namespace fileio {

// forward declarations
class FileRead;	// read only(Optical Disc drive : DVD, Blu-ray DVD...)
class File;		// general read/write file
template<typename file_traits> class basic_ifstream;

// input file stream 
typedef basic_ifstream<FileRead> ifstream;


}}}

#endif