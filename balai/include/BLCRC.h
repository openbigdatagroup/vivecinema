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
 * @file	BLCRC.h
 * @desc    Cyclic Redundancy Check/Codes/Checksum
 * @author	andre chen
 * @history	2012/01/04 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_CRC_H
#define BL_CRC_H

#include "BLCore.h"

namespace mlabs { namespace balai {

// bad(init) crc value
static uint32 const BL_BAD_CRC_VALUE = BL_BAD_UINT32_VALUE;

uint32 CalcCRC(uint8 const* data, uint32 len);
uint32 CalcCRC(char const* str, uint32* len=NULL);
uint32 CalcCICRC(char const* str, uint32* len=NULL);
uint32 CalcWordCRC(char const* str, uint32* len=NULL);
uint32 CalcFilenameCRC(char const* str, uint32* len=NULL);

}} // namespace nu

#endif