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
 * @file	BLArray.h
 * @desc    continuous, order container
 * @author	andre chen
 * @history	2011/12/28 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_TARRAY_H
#define BL_TARRAY_H

#include "BLMemory.h"
#include <algorithm> // std::sort

namespace mlabs { namespace balai {

// The class T is either native data or is class data that has the following
// member functions:
//   T::T(T const&);
//   T& T::operator=(T const&)
//   bool T::operator==(T const&) const

template <typename T, typename size_type=uint32>
class Array
{
	enum { MIN_CAPACITY_SIZE = (sizeof(T)<=4) ? (32/sizeof(T)):4 };
	size_type	size_;
	size_type	capacity_;
	size_type	grow_;
    T*			elements_;
	
	// allocate memory
	bool buy_(size_type capacity);

public:
	typedef T&		 reference;
	typedef T const& const_reference;
	typedef T*		 iterator;
	typedef T const* const_iterator;

	// default ctor creates an empty array
	Array():size_(0),capacity_(0),grow_(0),elements_(NULL) {}

	// reserve ctor's behavior is different from std::vector<>
    explicit Array(size_type reserved, size_type grow=MIN_CAPACITY_SIZE):
	size_(0),capacity_(0),grow_(grow<MIN_CAPACITY_SIZE ? 0:grow),elements_(NULL) {
		if (!buy_(reserved)) {
			BL_THROW_BADALLOC;
		}
	}

	// copy ctor
    Array(Array const& rhs);

	// dtor
	~Array() { cleanup(); }

	// assignment operator
	Array& operator=(Array const& rhs) {
		if (this!=&rhs) {
            Array tmp(rhs); // if it throws, nothing lose.
		    swap(tmp);
		}
		return *this;
	}
	
	// comparison
	bool operator==(Array const& rhs) const {
		if (size_!=rhs.size_)
			return false;
		
		if (this!=&rhs) {
            for (size_type i=0; i<size_; ++i)
                if (elements_[i]!=rhs.elements_[i])
                    return false;
		}
		return true;
	}
	bool operator!=(Array const& rhs) const {
		return !(rhs==*this);
	}

	// compound
	Array& operator+=(Array const& rhs) {
		if (0==rhs.size_)
			return *this; // no duplicate elements?

		// to prevent a += a;
		size_type const size_org    = size_; 
		size_type const size_added  = rhs.size_;
		size_type const size_totals = size_org + size_added;
		if (!buy_(size_totals)) {
			BL_THROW_BADALLOC;
		}

		BL_ASSERT(capacity_>=size_totals);
		size_type copied = 0;

		// placement new, no
		BL_TRY_BEGIN
			for (; copied<size_added; ++copied) {
				::new (&elements_[size_org + copied]) T(rhs.elements_[copied]);
			}
		BL_CATCH_ALL
			// roll back
			for (size_type i=0; i<copied; ++i) {
				elements_[size_org + copied].~T();
			}
			BL_RETHROW; // re-throw
		BL_CATCH_END

		size_ = size_totals;

		return *this; 
	}
	
	T& operator[](size_type i) {
		BL_ASSERT(i<size_);
		return elements_[i];
	}
	
	T const& operator[](size_type i) const {
		BL_ASSERT(i<size_);
		return elements_[i];
	}

	// nothrow swap
	void swap(Array& rhs) BL_EXCEPTION_SPEC_NOTHROW {
		size_type tmp = rhs.size_;
		rhs.size_ = size_;
		size_ = tmp;

		tmp = rhs.capacity_;
		rhs.capacity_ = capacity_;
		capacity_ = tmp;

		tmp = rhs.grow_;
		rhs.grow_ = grow_;
		grow_ = tmp;

		T*	tmpT = rhs.elements_;
		rhs.elements_ = elements_;
		elements_ = tmpT;
	}

	// clear and free allocated memory
	void cleanup() {
		for (size_type i=0; i<size_; ++i)
			elements_[i].~T();

		blFree(elements_);
		elements_ = NULL;
		size_ = capacity_ = 0;
	}

	// clear all elements but memory remain allocated
	void clear() {
		for (size_type i=0; i<size_; ++i)
			elements_[i].~T();
		size_ = 0;

		// Capacity not change
	}

	// element access
	bool empty() const			{ return (size_<1); }
	size_type size() const		{ return size_; }
	size_type capacity() const	{ return capacity_; }
	
	// begin and end
	T* begin()					{ return elements_; }
	T const* begin() const		{ return elements_; }
	T* end()					{ return elements_ + size_; } /*  invalid pointer */
	T const* end() const		{ return elements_ + size_; } /*  invalid pointer */

	T* at(size_type where_)	{
		return (size_<=where_) ? NULL:(elements_ + where_);
	}
	T const* at(size_type where_) const	{
		return (size_<=where_) ? NULL:(elements_ + where_);
	}
	T* at_reverse(size_type where_)	{
		return (size_<=where_) ? NULL:(elements_ + (size_ - where_ - 1));
	}
	T const* at_reverse(size_type where_) const	{
		return (size_<=where_) ? NULL:(elements_ + (size_ - where_ - 1));
	}

	// front and back
	T& front() {
		BL_ASSERT(size_>0);
		return elements_[0];
	}
	T const& front() const {
		BL_ASSERT(size_>0);
		return elements_[0];
	}
	T& back() {
		BL_ASSERT(size_>0);
		return elements_[size_-1];
	}
	T const& back() const {
		BL_ASSERT(size_>0);
		return elements_[size_-1];
	}

	// reserve enough slots ready to use
	bool reserve(size_type cap) {
		return buy_(cap);
	}

	// add new element, array will dynamically grow if necessary
	size_type push_back(T const& e) { // may throw
		if (!buy_(size_+1)) {
			BL_THROW_BADALLOC;
		}

		::new (&elements_[size_]) T(e);
		return (++size_);
	}
	
	size_type pop_back() {
		if (size_>0)
			elements_[--size_].~T();
		return size_;
	}

	// i don't like the idea of push_front()/pop_front()
	size_type push_front(T const& e) { insert(0, e);    return size_; }
	size_type pop_front()			 { remove(0, true); return size_; }

	// reallocate so there is no wasted space (size_ = capacity_)
	void shrink();
	void insert(size_type id, T const& e);
	bool erase(T const& e, bool bKeepOrder = false);

	bool remove(size_type index, bool bKeepOrder=false);
	
	// range remove - return the size removed and always keep order
	size_type remove(size_type index, size_type num); 

	template<typename uni_func>
	size_type remove_if(uni_func& pred, bool bKeepOrder=false) {
		for (size_type i=0; i<size_;) {
			if (pred(elements_[i])) {
				if (!remove(i, bKeepOrder)) {
					BL_HALT("remove error!?"); // ERROR!?
					return size_;
				}
			}
			else {
				 ++i;
			}
		}

		return size_;
	}

	// traverse of all items
	template<typename uni_func>
	size_type for_each(uni_func const& pred, bool reversed = false) {
		if (reversed) {
			for (size_type i=0; i<size_; ++i) {
				if (!pred(elements_[size_-i-1]))
					return i;
			}
		}
		else {
			for (size_type i=0; i<size_; ++i) {
				if (!pred(elements_[i]))
					return i;
			}
		}

		return size_;
	}
	template<typename uni_func>
	size_type for_each(uni_func& pred, bool reversed = false) {
		if (reversed) {
			for (size_type i=0; i<size_; ++i) {
				if (!pred(elements_[size_-i-1]))
					return i;
			}
		}
		else {
			for (size_type i=0; i<size_; ++i) {
				if (!pred(elements_[i]))
					return i;
			}
		}

		return size_;
	}

	// sort
	void sort() { // if class T has "bool operator<"
		if (elements_ && size_>1)
			std::sort(elements_, elements_+size_); 
	}
	template<typename predicate> // if no operator<, provide compare predicator please!
	void sort(predicate& comp) {
		if (elements_ && size_>1)
			std::sort(elements_, elements_+size_, comp); 
	}
	template<typename predicate> // if no operator<, provide compare predicator please!
	void sort(predicate const& comp) {
		if (elements_ && size_>1)
			std::sort(elements_, elements_+size_, comp); 
	}
};

//----------------------------------------------------------------------------
template <typename T, typename size_type>
bool Array<T, size_type>::buy_(size_type capacity)
{
	if (capacity>capacity_) {
		if (NULL==elements_) {
			BL_ASSERT(0==size_ && 0==capacity_);
			if (capacity<MIN_CAPACITY_SIZE)
				capacity = MIN_CAPACITY_SIZE;

			elements_ = reinterpret_cast<T*>(blMalloc(capacity*sizeof(T)));
			if (NULL==elements_) {
				return false;
			}
			capacity_ = capacity;
			return true;
		}

		// Grow array
		BL_ASSERT(0!=capacity_);

		if (grow_>0) {
			if (capacity<(capacity_+grow_))
				capacity = (capacity_+grow_);
		}
		else {
			// grow 50%
			if (capacity<(capacity_+(capacity_>>1)))
				capacity = (capacity_+(capacity_>>1));
		}

		// if there is not enough available memory to expand the block,
		// the given size of the original block is unchanged.
        T* new_elements = reinterpret_cast<T*>(blRealloc(elements_, capacity*sizeof(T)));
        if (NULL==new_elements)
            return false;

        elements_ = new_elements;
        capacity_ = capacity;
	}

    return true;
}
//----------------------------------------------------------------------------
template <typename T, typename size_type>
Array<T, size_type>::Array(Array const& rhs):
size_(0),
capacity_(0),
grow_(rhs.grow_),
elements_(NULL)
{
	if (0==rhs.size_)
		return;

	if (!buy_(rhs.size_)) {
		BL_THROW_BADALLOC;
	}

	BL_TRY_BEGIN
		for (size_=0; size_<rhs.size_; ++size_)
			::new (&elements_[size_]) T(rhs.elements_[size_]);
	BL_CATCH_ALL
		// roll back
		for (size_type i=0; i<size_; ++i) {
			elements_[i].~T();
		}
		size_ = 0;
		BL_RETHROW;
	BL_CATCH_END
}
//----------------------------------------------------------------------------
template <typename T, typename size_type>
void Array<T, size_type>::shrink()
{
	if (size_==capacity_)
		return;

	if (0==size_) {
		blFree(elements_);
		elements_ = NULL;
		capacity_ = 0;
		return;
	}

    void* NewElementArray = blMalloc(size_*sizeof(T));
    if (NULL==NewElementArray)
        return;

	std::memcpy(NewElementArray, elements_, size_*sizeof(T));
	blFree(elements_);
    elements_ = reinterpret_cast<T*>(NewElementArray);
    capacity_ = size_;
}
//----------------------------------------------------------------------------
template <typename T, typename size_type>
void Array<T, size_type>::insert(size_type id, T const& e)
{
	if (id>=size_) {	
		push_back(e);
		return;
	}

	if (!buy_(size_+2)) { // an extra space to temporary hold new T
		BL_THROW_BADALLOC;
	}

	// try to construct a new object
	::new (&elements_[size_+1]) T(e); // this may throw

	// memory shift
	std::memmove(&elements_[id+1], &elements_[id], (size_-id)*sizeof(T));

	// copy to the right position
	std::memcpy(&elements_[id], &elements_[size_+1], sizeof(T));

	++size_;

	return;
}
//----------------------------------------------------------------------------
template <typename T, typename size_type>
bool Array<T, size_type>::erase(T const& e, bool bKeepOrder)
{
	for (size_type i=0; i<size_; ++i) {
		if (elements_[i]!=e) 
			continue;
		
		// Destruct the element to be removed
		elements_[i].~T();

		size_type const last_id = size_ - 1;
		if (i!=last_id) {
			if (bKeepOrder)
				std::memmove(&elements_[i], &elements_[i+1], (last_id-i)*sizeof(T));
			else
				std::memcpy(&elements_[i], &elements_[last_id], sizeof(T));
		}
			
		size_ = last_id;
		return true;
	}

	return false;
}
//-----------------------------------------------------------------------------
template <typename T, typename size_type>
bool Array<T, size_type>::remove(size_type id, bool bKeepOrder)
{
	if (id<size_) {
		// Destruct the element to be removed
		elements_[id].~T();

		size_type const last_id = size_ - 1;
		if (id!=last_id) {
			if (bKeepOrder)
				std::memmove(&elements_[id], &elements_[id+1], (last_id-id)*sizeof(T));
			else
				std::memcpy(&elements_[id], &elements_[last_id], sizeof(T));
		}

		size_ = last_id;
		return true;
	}
	return false;
}
//-----------------------------------------------------------------------------
template <typename T, typename size_type>
size_type Array<T, size_type>::remove(size_type id, size_type num)
{
	if (id<size_) {
		size_type const back = id; 
		for (size_type i=0; i<num; ++i, ++id) {
			if (id>=size_) {
				size_ = back;
				return i;
			}

			// Destruct the element to be removed
			elements_[id].~T();
		}

		// fill the hole
		if (id<size_) {
			std::memmove(&elements_[back], &elements_[id], (size_-id)*sizeof(T));
		}

		size_ -= num;
		return num;
	}
	return 0;
}

// TArray<T*>::for_each(safe_delete_functor()), to delete all pointer elements
struct safe_delete_functor {
	template<typename T>
	bool operator()(T*& ptr) const {
		delete ptr;
		ptr = NULL;
		return true;
	}
};

// TArray<T*>::for_each(safe_array_delete_functor()), to array-delete all array-pointer elements
struct safe_array_delete_functor {
	template<typename T>
	bool operator()(T*& ptr) const {
		delete[] ptr;
		ptr = NULL;
		return true;
	}
};

// TArray<T*>::for_each(safe_release_functor()), to release all resource elements
struct safe_release_functor {
	template<typename T>
	bool operator()(T*& res) const {
		if (res) {
			res->Release();
			res = NULL;
		}
		return true;
	}
};

}} // mlabs::balai

#endif // BL_TARRAY_H
