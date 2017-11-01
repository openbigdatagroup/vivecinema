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
 * @file	BLObject.h
 * @desc    base object
 * @author	andre chen
 * @history	2011/12/30 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_OBJECT_H
#define BL_OBJECT_H

#include "BLMemory.h"

namespace mlabs { namespace balai {

//- simple RTTI for Objects ---------------------------------------------------
class Rtti
{
private:
    char const* const kName_;		// #classname, string literal
    Rtti const* const kBaseType_;	// const pointer point to const Rtti

	// disable default/copy ctor and assignment operator, prevent alloc from heap
	BL_NO_DEFAULT_CTOR(Rtti);
	BL_NO_COPY_ALLOW(Rtti);
	BL_NO_HEAP_ALLOC();

public:
    // Name must be unique among all objects in the system.
	Rtti(char const* classname, Rtti const* pkBaseType):
	  kName_(classname),kBaseType_(pkBaseType) {}
	~Rtti() {}

	char const* GetName() const				 { return kName_; }
	bool IsExactly(Rtti const& rkType) const { return (&rkType==this); }
	bool IsDerived(Rtti const& rkType) const {
		Rtti const* pkSearch = this;
		while (pkSearch) {
			if (pkSearch->IsExactly(rkType))
				return true;

			pkSearch = pkSearch->kBaseType_;
		}
		return false;
	}
};

/**
 * IObject - interface for all balai objects
 */
class IObject
{
	// RTTI & DoClone
	virtual Rtti const& RTTI_() const = 0;
	virtual IObject* DoClone_() const = 0;

protected:
	IObject() {}
	IObject(IObject const&) {}
	IObject& operator=(IObject const&) { return *this; }

public:
	virtual ~IObject() {}

	// run-time type information system
	static Rtti const TYPE;
	char const* ClassName() const		{ return RTTI_().GetName(); }
	bool IsExactly(Rtti const& t) const	{ return RTTI_().IsExactly(t); }
	bool IsKindOf(Rtti const& t) const	{ return RTTI_().IsDerived(t); }
	bool IsExactlyTypeOf(IObject const* obj) const {
		return obj && RTTI_().IsExactly(obj->RTTI_());
	}
    bool IsDerivedTypeOf(IObject const* obj) const {
	   return obj && RTTI_().IsDerived(obj->RTTI_());
	}

	// Clone
	IObject* Clone() const {
		BL_TRY_BEGIN
			IObject* pObj = DoClone_();
			if (NULL==pObj)
				return NULL; // this already indicates an error(or on purpose!)

			BL_ASSERT2(RTTI_().IsExactly(pObj->RTTI_()), "cloned Object has wrong type!");
			return pObj;
		BL_CATCH_ALL
			return NULL;
		BL_CATCH_END
	}

	// Q. why we need these?
	// A. for some platforms we can not overwrite global operator new/delete,
	//    so we use object operator new/delete
#ifdef BL_TRACKING_MEMORY
public:
	// operator new/delete operators are explicit 'static' methods.
	void* operator new(std::size_t size) BL_EXCEPTION_SPEC_BADALLOC;
	void* operator new[](std::size_t size) BL_EXCEPTION_SPEC_BADALLOC;
	void operator delete(void* ptr) BL_EXCEPTION_SPEC_NOTHROW {
		system::IAllocator::free(ptr, system::MEM_CMD_NEW);
	}
	void operator delete[](void* ptr) BL_EXCEPTION_SPEC_NOTHROW {
		system::IAllocator::free(ptr, system::MEM_CMD_ARRAY_NEW);
	}

	// no-throw version
	void* operator new(std::size_t size, std::nothrow_t const&) BL_EXCEPTION_SPEC_NOTHROW;
	void* operator new[](std::size_t size, std::nothrow_t const&) BL_EXCEPTION_SPEC_NOTHROW;
	void operator delete(void* ptr, std::nothrow_t const&) BL_EXCEPTION_SPEC_NOTHROW {
		system::IAllocator::free(ptr, system::MEM_CMD_NEW);
	}
	void operator delete[](void* ptr, std::nothrow_t const&) BL_EXCEPTION_SPEC_NOTHROW {
		system::IAllocator::free(ptr, system::MEM_CMD_ARRAY_NEW);
	}

	// memory tracking version
	void* operator new(std::size_t size, char const* file, int line) BL_EXCEPTION_SPEC_BADALLOC;
	void* operator new[](std::size_t size, char const* file, int line) BL_EXCEPTION_SPEC_BADALLOC;
	void operator delete(void* ptr, char const*, int) BL_EXCEPTION_SPEC_NOTHROW {
		system::IAllocator::free(ptr, system::MEM_CMD_NEW);
	}
	void operator delete[](void* ptr, char const*, int) BL_EXCEPTION_SPEC_NOTHROW {
		system::IAllocator::free(ptr, system::MEM_CMD_ARRAY_NEW);
	}
#endif
};

/**
 * IShared - reference counted object
 * Important!
 * delete IShared is protected. BUT since IShared inherited from IObject,
 * there are still chances to delete IShared via IObject dtor.
 * Don't do that!
 */
class IShared : public IObject  
{
	virtual Rtti const& RTTI_() const { return TYPE; }

	// Reference count of this "Shared", create with 1
    // TO-DO :
    //mutable std::atomic<int> refCount_;
	mutable volatile int refCount_;

	// disable copy ctor and assignment operator
	BL_NO_COPY_ALLOW(IShared);

protected:
	IShared():refCount_(1) {}

	// Sutter suggests that a dtor must be either 
	//	1) public & virtual or 
	//	2) protected & non-virtual.
	// But this is not the case, all IShared objects must have dtor protected.
	// It prevents client to delete SharedObject directly (delete via Release)
	virtual ~IShared() = 0; // pure virtual class

public:
	// Clone - Shared Object version.
	IObject* DoClone_() const {
		BL_ASSERT(refCount_>0); // clone a zombie?
		++refCount_;

		// It's UGLY! but i'm pretty sure that "this" isn't const really.
		return const_cast<IShared*>(this); 
	}

	// smart pointer system
	int RefCount() const { return refCount_; }
	int AddRef() const			{ 
		BL_ASSERT(refCount_>0); // you see dead people?
		return ++refCount_; 
	}
	int Release() {
		BL_ASSERT(refCount_>0); // you really did?
		if (--refCount_>0)
			return refCount_;

		delete this;	// R.I.P.
		return 0;
	}

	// rtti
	static Rtti const TYPE;
};
//----------------------------------------------------------------------------
inline IShared::~IShared() { BL_ASSERT(refCount_<=0); }


/**
 * IResouce - i.e. IShared that have name
 */
class IResource : public IShared
{
	// rtti
	virtual Rtti const& RTTI_() const { return TYPE; }

	uint32 const id_;
	uint32 const group_;

	// disable default/copy ctor and assignment operator
	BL_NO_DEFAULT_CTOR(IResource);
	BL_NO_COPY_ALLOW(IResource);

protected:
	IResource(uint32 id, uint32 group):id_(id),group_(group) {}
	virtual ~IResource() = 0; // make it a pure virtual class

public:
	uint32 Name() const	 { return id_; }
	uint32 Group() const { return group_; }

	// rtti
	static Rtti const TYPE;
};
//-----------------------------------------------------------------------------
inline IResource::~IResource() {}

/*****************************************************************************
* [16.3.2] The # operator (cpp.stringize)                                   *
* 1. Each # preprocessing token in the replacement list for a function-like *
*    macro shall be followed by a parameter as the next preprocessing token *
*    in the replacement list.                                               *
* 2. If, in the replacement list, a parameter is immediately proceded by a  *
*    # preprocessing token, both are replaced by a single character         *
*    string literal preprocessing token that contains the spelling of the   *
*    ^^^^^^^^^^^^^^                                                         *
*    preprocessing token sequence for the corresponding argument......      *
*****************************************************************************/
#define BL_DECLARE_RTTI \
public: \
    static Rtti const TYPE; \
private: \
	virtual Rtti const& RTTI_() const { return TYPE; }

//----------------------------------------------------------------------------
#define BL_IMPLEMENT_RTTI(classname,baseclassname) \
Rtti const classname::TYPE(#classname,&baseclassname::TYPE);


//- safe downcast -------------------------------------------------------------
template <typename T, typename U>
T* SafeDownCast(U* pObj) {
	if (0==pObj || !pObj->IsKindOf(T::TYPE))
		return 0;
	
	return static_cast<T*>(pObj); 
}

}} // namespace mlabs::balai

#endif // BL_OBJECT_H