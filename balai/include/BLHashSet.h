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
 * @file	BLHashSet.h
 * @desc    continuous, un-order set
 * @author	andre chen
 * @history	2011/12/28 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_THASHSET_H
#define BL_THASHSET_H

#include "BLArray.h"

namespace mlabs { namespace balai {

/*
Knuth's Multiplicative Method(http://www.concentric.net/~Ttwang/tech/inthash.htm)
In Knuth's "The Art of Computer Programming", section 6.4, a multiplicative 
hashing scheme is introduced as a way to write hash function. The key is 
multiplied by the golden ratio of 2^32 (2654435769) to produce a hash result. 

Since 2654435769 and 2^32 has no common factors in common, the multiplication 
produces a complete mapping of the key to hash result with no overlap. 
This method works pretty well if the keys have small values. Bad hash results 
are produced if the keys vary in the upper bits. As is true in all 
multiplications, variations of upper digits do not influence the lower digits 
of the multiplication result. 
*/

inline std::size_t HashIndex(std::size_t index, std::size_t tableSize)
{
/*
	static const double s_dHashMultiplier = 0.5*(std::sqrt(5.0)-1.0);
	
	// The fmod(x,y) function calculates the floating-point remainder f of x / y 
	// such that x = i * y + f, where i is an integer, f has the same sign as x, 
	// and the absolute value of f is less than the absolute value of y.
	return (std::size_t) std::floor(tableSize*std::fmod(s_dHashMultiplier*(index%tableSize), 1.0));
*/
#if 0	
	// original version
	BL_ASSERT(0==(tableSize&(tableSize-1)));
	uint64 const res = uint64(index)*2654435769UL;
	return (std::size_t)(res&(tableSize-1));
#else
	// simplified version - since you don't really care how big multiplicatation is.
	BL_ASSERT(0==(tableSize&(tableSize-1)));
	BL_ASSERT(((uint64(index)*2654435769UL)&(tableSize-1))==((index*2654435769UL)&(tableSize-1)));
	return (index*2654435769UL)&(tableSize-1);
#endif
}

template <typename element, std::size_t TABLE_SIZE>
class HashSet
{
	// declare not defined
	BL_NO_COPY_ALLOW(HashSet);

	struct HashItem {
        element		Element;
        HashItem*	Next;
    };

	typedef std::size_t (*KEYFUNCTION)(element const& ele);

    BL_COMPILE_ASSERT(TABLE_SIZE>=64, hashset_tablesize_is_less_than_64);
	BL_COMPILE_ASSERT((TABLE_SIZE&(TABLE_SIZE-1))==0, hashset_tablesize_must_be_power_of_2);
	enum { ALLOC_ARRAY_SIZE = 64 };
	HashItem*			table_[TABLE_SIZE];
	std::size_t			size_;
	HashItem*			freeList_;
	Array<HashItem*>	allocBuffers_;
	KEYFUNCTION			KeyFunction_;

    // iterator for traversal
	mutable std::size_t	index_;		// current table index
    mutable HashItem*	current_;	// current list node

	// new hash item
	HashItem* new_hash_item_() {
		HashItem* item = freeList_;
		if (item) {
			freeList_ = freeList_->Next;
			item->Next = NULL;
			return item;
		}

		BL_TRY_BEGIN
			item = blNew HashItem[ALLOC_ARRAY_SIZE];
			allocBuffers_.push_back(item);
			
			// one return
			item->Next = NULL;

			// others link altogether and become new freelist_
			for (int i=1; i<ALLOC_ARRAY_SIZE; ++i) {
				item[i].Next = freeList_;
				freeList_ = item + i;
			}
		BL_CATCH_ALL
			return NULL;
		BL_CATCH_END
		return item;
	}
	//
	void recycle_item_(HashItem* pItem) {
		if (pItem) {
			pItem->Next = freeList_;
			freeList_ = pItem;
		}
	}

public:
	explicit HashSet(KEYFUNCTION KeyFunc=NULL):
	size_(0),freeList_(NULL),allocBuffers_(16),KeyFunction_(KeyFunc),index_(0),current_(NULL) {
		std::memset(table_, 0, TABLE_SIZE*sizeof(HashItem*));
	}
	~HashSet() { clear(); }

	// clear
	void clear() {
		size_	  = 0;
		freeList_ = NULL;
		index_	  = 0;
		current_  = NULL;
		std::memset(table_, 0, TABLE_SIZE*sizeof(HashItem*));
		while (!allocBuffers_.empty()) {
			HashItem* buf = allocBuffers_.back();
			allocBuffers_.pop_back();
			delete[] buf;
		}
	}

	// reserve space for extra items to save
	bool reserve(uint32 reserveExt) {
		uint32 cap = 0;
		HashItem* item = freeList_;
		while (item && cap<reserveExt) {
			if (++cap>=reserveExt) 
				return true;
			item = item->Next;
		}

		if (cap>=reserveExt) 
			return true;

		BL_TRY_BEGIN
			reserveExt -= cap;
			reserveExt = BL_ALIGN_UP(reserveExt, 8);
			item = blNew HashItem[reserveExt];
			allocBuffers_.push_back(item);
			for (uint32 i=0; i<reserveExt; ++i) {
				item[i].Next = freeList_;
				freeList_ = item + i;
			}
			return true;
		BL_CATCH_ALL
			return false;
		BL_CATCH_END
	}

    // element access
	std::size_t size() const { return size_; }
	bool empty() const { return (0==size_); }
    
	// return false if equivalent element had been stored
    bool insert(element const& ele) {
		std::size_t const hash = HashIndex(KeyFunction_? KeyFunction_(ele):(std::size_t)ele, TABLE_SIZE);
		HashItem* item = table_[hash];

		while (item) {
			// this is not an error, we have store this item...
			if (ele==item->Element)
				return false;

			item = item->Next;
		}

		// add item to beginning of this pile
		item = new_hash_item_();
		item->Element = ele;
		item->Next	  = table_[hash];
		table_[hash]  = item;
		++size_;
		return true;
	}

    // find the equivalent element the set had stored
	element* find(element const& ele) const {
		HashItem* item = table_[HashIndex(KeyFunction_? KeyFunction_(ele):(std::size_t)ele, TABLE_SIZE)];

		// search for item in list associated with key
		while (item) {
			if (ele==item->Element) {
				// item is in hash table
				return &(item->Element);
			}
			item = item->Next;
		}
		return NULL;
	}
	
	// remove
	bool remove(element const& ele) {
		HashItem** ppItem = &(table_[HashIndex(KeyFunction_? KeyFunction_(ele):(std::size_t)ele, TABLE_SIZE)]);
		while (*ppItem) {
			HashItem* item = *ppItem;
			if (ele==item->Element) {
				*ppItem = item->Next;
				recycle_item_(item);
				--size_;
				return true;
			}	
			ppItem = &(item->Next);
		}
		return false;
	}

    /* call like this...
		element* ele = HashSet.first();
		while (ele) {
			// your code here...

			ele = HashSet.next();
		}
	*/
	element first() const {
		current_ = NULL;
		index_   = 0;
		if (0==size_)
			return NULL;
		for (; index_<TABLE_SIZE; ++index_) {
			if (table_[index_]) {
				current_ = table_[index_];
				return current_->Element;
			}
		}
		return NULL;
	}

    element next() const {
		BL_ASSERT(NULL!=current_);
		current_ = current_->Next;
		if (current_)
			return current_->Element;
    
		for (++index_; index_<TABLE_SIZE; ++index_) {
			if (table_[index_]) {
				current_ = table_[index_];
				return current_->Element;
			}
		}
		return NULL;
	}
};

}}
#endif
