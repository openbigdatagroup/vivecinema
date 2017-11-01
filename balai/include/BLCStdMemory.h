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
 * @file	BLCSDMemory.h
 * @desc    memory management standard C implement use malloc/delete
 * @author	andre chen
 * @history	2011/12/28 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */

#ifndef BL_CSTD_MEMORY_H
#define BL_CSTD_MEMORY_H

#include "BLMemory.h"
#include <cstdlib>	 // std::malloc/std::free

namespace mlabs { namespace balai { namespace system {

//- standard C98/99 memory manager, implemented use std::malloc/std::free -----
class MemoryCStd : private IAllocator
{
    void* heap_;
    size_t main_pool_size_, sub_pool_size_;

	// mlAllocator
	void OnInit_() {
		// NOTE : Mutex is locked... don't do anything stupid!
		// precondition
		BL_ASSERT(NULL==heap_);
		
		// main heap
		size_t const size = main_pool_size_ + sub_pool_size_;
		heap_ = std::malloc(size);
		if (heap_) {
			InitMemPool_(heap_, main_pool_size_, ((uint8*)heap_)+main_pool_size_, sub_pool_size_);
//			BL_LOG("[balai:mem] (info) %iMB main pool + %iKB sub pool created\n\n", ((MAIN_POOL_SIZE & 0xFFF00000)>>20), (SUB_POOL_SIZE>>10));
			return;
		}

		// fail to allocate main memory
		BL_ERR("[balai:mem] (error) fail to create main memory pool(size=%i), App must quit!!!\n", size);
		BL_HALT("R.I.P.");
	}
	void OnOutOfMemory_(uint32 /*required_size*/) {
		BL_ERR("[balai:mem] (error) on out-of-memory\n");
	}

    // not defined
    MemoryCStd();

protected:
    explicit MemoryCStd(size_t main_size, size_t sub_size=65536):IAllocator(),heap_(NULL) {
        main_pool_size_ = main_size;
        sub_pool_size_ = sub_size;
    }
	~MemoryCStd() {
		// dump memory still allocated
		IAllocator::FinishUp_();

		// free all
		if (heap_)
			std::free(heap_);

		heap_ = NULL;
	}
	friend class IAllocator;
};

}}}

//- memory manager implementation macro ---------------------------------------
#define BL_CSTD_MEMMGR_IMPLMENTATION(main_pool_size, sub_pool_size) \
namespace mlabs { namespace balai { namespace system { \
IAllocator& IAllocator::GetInstance() { \
	static MemoryCStd memallocator_(main_pool_size, sub_pool_size); \
	return memallocator_; \
} \
}}}

#endif // BL_CSTD_MEMORY_H
