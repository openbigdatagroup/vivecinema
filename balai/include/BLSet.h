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
 * @file	set.h
 * @desc    continuous, distinct array
 * @author	andre chen
 * @history	2011/12/28 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_TSET_H
#define BL_TSET_H

#include "BLArray.h"

namespace mlabs { namespace balai {

template <typename T>
class Set
{
	Array<T> array_;

public:
	explicit Set(int reserved=0):array_(reserved) {}
	~Set() {}

	uint32 size() const				 { return array_.size(); }
	T& operator[](int i)			 { return array_[i]; }
	T const& operator[](int i) const { return array_[i]; }

    // insertion, removal, searching
	bool insert(T const& t) {
		uint32 const size = array_.size();
		for (uint32 i=0; i<size; ++i)
			if (t==array_[i])
				return true;
		BL_TRY_BEGIN
			array_.push_back(t);
			return true;
		BL_CATCH_ALL
            //...
		BL_CATCH_END
	    return false;
	}

	bool remove(T const& t) {
		uint32 const size = array_.size();
		for (uint32 i=0; i<size; ++i) {
			if (t==array_[i]) {
				array_.remove(i, false);
				return true;
			}
		}
		return false;
	}

	bool exists(T const& t) {
		uint32 const size = array_.size();
		for (uint32 i=0; i<size; ++i)
			if (t==array_[i])
				return true;
		return false;
	}

    // make empty set, keep quantity and growth parameters
	void clear() { array_.clear(); }
};

}}
#endif
