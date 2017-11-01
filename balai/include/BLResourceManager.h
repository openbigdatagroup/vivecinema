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
 * @file	BLResourceManager.h
 * @desc    resource manager
 * @author	andre chen
 * @history	2011/12/30 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_RESOURCE_MANAGER_H
#define BL_RESOURCE_MANAGER_H

#include "BLHashTable.h"

namespace mlabs { namespace balai {

template<typename T, uint32 TableSize>
class ResourceManager
{
public:
	typedef T ResourceType;

private:
	HashTable<uint32, ResourceType*, TableSize> resourceTable_;

	// disable copy ctor and assignment operator
	BL_NO_COPY_ALLOW(ResourceManager);

protected:
	ResourceManager():resourceTable_() {}
	
	// not necessary be virtual since it's not public
	virtual ~ResourceManager() {
		ResourceType* res = resourceTable_.first();
		while (res) {
//			BL_ASSERT(1==res->ReferenceCount()); // may be hold by others!
			res->Release();
			res = resourceTable_.next();
		}
		resourceTable_.clear();
	}

public:
	template<typename _Callback>
	uint32 For_Each(_Callback fun) {
		uint32 succeed = 0;
		ResourceType* res = resourceTable_.first();
		while (res) {
			if (!fun(res))
				return succeed;

			++succeed;
			res = resourceTable_.next();
		}
		return succeed;
	}

	uint32 ReleaseAll() {
		uint32 succeed = 0;
		ResourceType* res = resourceTable_.first();
		while (res) {
			res->Release();
			res = resourceTable_.next();
			++succeed;
		}
		resourceTable_.clear();
		return succeed;
	}

	// it AddRef() if found, fail to release resource results resource leak
	ResourceType* Find(uint32 name) const {
		ResourceType* res = NULL;
		if (!resourceTable_.find(name, &res))
			return NULL;

		res->AddRef();
		return res;
	}

	bool Register(ResourceType* res) {
		if (NULL==res || !resourceTable_.insert(res->Name(), res))
			return false;
		BL_ASSERT(0!=res->Name());
		res->AddRef();
		return true;
	}
	
	bool UnRegister(ResourceType* res) {
		if (NULL!=res) {
			uint32 const name = res->Name();
			ResourceType* res2 = NULL;
			if (resourceTable_.find(name, &res2)) {
				BL_ASSERT(res2==res);
				res2->Release();
				if (!resourceTable_.remove(name)) {
					BL_HALT("ResourceManager::UnRegister");
				}
				return true;
			}
		}
		return false;
	}

	bool UnregisterByName(uint32 name) {
		ResourceType* res = NULL;
		if (!resourceTable_.find(name, &res)) 
			return false;

		BL_ASSERT(res);
		res->Release();
		if (!resourceTable_.remove(name)) {
			BL_HALT("ResourceManager::UnregisterByName");
		}
		return true;
	}
	
	uint32 UnregisterByGroupId(uint32 groupID) {
		uint32 succeed = 0;
		ResourceType* res = resourceTable_.first();
		while (res) {
			ResourceType* next = resourceTable_.next();
			BL_ASSERT(res);
			if (groupID==res->Group()) {
				if (!resourceTable_.remove(res->Name())) {
					BL_HALT("ResourceManager::UnregisterByGroupId");
				}
				res->Release();
				++succeed;
			}
			res = next;
		}
		return succeed;
	}

	template<typename predicator>
	uint32 Unregister(predicator pred) {
		uint32 succeed = 0;
		ResourceType* res = resourceTable_.first();
		while (res) {
			ResourceType* next = resourceTable_.next();
			if (pred(res)) {
				if (!resourceTable_.remove(res->Name())) {
					BL_HALT("ResourceManager::Unregister(pred)");
				}
				res->Release();
				++succeed;
			}
			res = next;
		}
		return succeed;
	}

	uint32 Size() const { return (uint32) resourceTable_.size(); }
};

}}

#endif // BL_RESOURCE_MANAGER_H