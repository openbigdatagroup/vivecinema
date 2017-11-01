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
 * @file	BLHashTable.h
 * @desc    continuous, un-order map
 * @author	andre chen
 * @history	2011/12/28 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_THASHTABLE_H
#define BL_THASHTABLE_H

#include "BLHashSet.h"

namespace mlabs { namespace balai {

template <typename TKEY, typename TVALUE, std::size_t TABLE_SIZE>
class HashTable
{
	// disable copy ctor and assignment operator
	BL_NO_COPY_ALLOW(HashTable);
    
	struct HashItem {
        TKEY		Key;
        TVALUE		Value;
        HashItem*	Next;
    };

	BL_COMPILE_ASSERT(TABLE_SIZE>=64, HashTable_tablesize_is_less_than_64);
	BL_COMPILE_ASSERT((TABLE_SIZE&(TABLE_SIZE-1))==0, HashTable_tablesize_must_be_power_of_2);
	enum { ALLOC_ARRAY_SIZE = 64 };

	// hash table
	HashItem*        table_[TABLE_SIZE];
	Array<HashItem*> allocBuffers_;
	HashItem*        freeList_;
	std::size_t      size_;
	
    // iterator for traversal
	mutable std::size_t	index_;		// current table index
    mutable HashItem*	current_;	// current list node

	// new hash item
	HashItem* new_hash_item_() {
		HashItem* item = freeList_;
		if (item) {
			freeList_ = item->Next;
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
	explicit HashTable(uint32 reserve=0):allocBuffers_(8),freeList_(NULL),size_(0),index_(0),current_(NULL) {
		std::memset(table_, 0, TABLE_SIZE*sizeof(HashItem*));
		if (reserve>0) {
			reserve = BL_ALIGN_UP(reserve, 8);
			HashItem* alloc = blNew HashItem[reserve]; // may throw
			for (uint32 i=0; i<reserve; ++i) {
				alloc[i].Next = freeList_;
				freeList_ = alloc + i;
			}
			allocBuffers_.push_back(alloc);
		}
	}
    ~HashTable() { clear(); }

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

	// reserve
	bool reserve(uint32 reserveExt) {
		HashItem* item = freeList_;
		uint32 cap = 0;
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

    // insert a key-value pair into the hash table
	bool insert(TKEY const& rkKey, TVALUE const& rkValue) {
		std::size_t const hash = HashIndex((std::size_t)rkKey, TABLE_SIZE);
		HashItem* item = table_[hash];

		// search for item in list associated with key
		while (item) {
			if (rkKey==item->Key) {
				// item already in hash table
				return (rkValue==item->Value);
			}
			item = item->Next;
		}

		// add item to beginning of this pile
		item		 = new_hash_item_();
		item->Key    = rkKey;
		item->Value  = rkValue; 
		item->Next	 = table_[hash];
		table_[hash] = item;
		++size_;
		return true;
	}

    // search for a key and returns its value (0, if key does not exist)
	bool find(TKEY const& rkKey, TVALUE* value=NULL) const {
		HashItem* item = table_[HashIndex((std::size_t)rkKey, TABLE_SIZE)];

		// search for item in list associated with key
		while (item) {
			if (rkKey==item->Key) {
				if (value)
					*value = item->Value;
				return true;
			}
			item = item->Next;
		}
		return false;
	}

    // remove key-value pairs from the hash table
	bool remove(TKEY const& rkKey) {
		HashItem** ppItem = &(table_[HashIndex((std::size_t)rkKey, TABLE_SIZE)]);
		while (*ppItem) {
			HashItem* cur = *ppItem;
			if (rkKey==cur->Key) {
				*ppItem = cur->Next;
				recycle_item_(cur);
				--size_;
				return true;
			}		
			ppItem = &(cur->Next);
		}
		return false;
	}

    // linear traversal of table
	TVALUE first(TKEY& rKey) const {
		current_ = NULL;
		index_   = 0;
		if (0==size_)
			return NULL;

		for (; index_<TABLE_SIZE; ++index_) {
			if (table_[index_]) {
				current_ = table_[index_];
				rKey = current_->Key;
				return current_->Value;
			}
		}
		return NULL;
	}

	TVALUE next(TKEY& rKey) const {
		BL_ASSERT(NULL!=current_);
		current_ = current_->Next;
		if (current_)
			return current_->Value;
    
		for (++index_; index_<TABLE_SIZE; ++index_) {
			if (table_[index_]) {
				current_ = table_[index_];
				rKey = current_->Key;
				return current_->Value;
			}
		}
		return NULL;
	}

	TVALUE first() const {
		current_ = NULL;
		index_   = 0;
		if (0==size_)
			return NULL;

		for (; index_<TABLE_SIZE; ++index_) {
			if (table_[index_]) {
				current_ = table_[index_];
				return current_->Value;
			}
		}
		return NULL;
	}

	TVALUE next() const {
		BL_ASSERT(NULL!=current_);
		current_ = current_->Next;
		if (current_)
			return current_->Value;
    
		for (++index_; index_<TABLE_SIZE; ++index_) {
			if (table_[index_]) {
				current_ = table_[index_];
				return current_->Value;
			}
		}
		return NULL;
	}
};

}}

#endif
