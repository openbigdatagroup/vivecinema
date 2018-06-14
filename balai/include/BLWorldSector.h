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
 * @file	BLWorldSector.h
 * @author	andre chen
 * @history	2012/02/24 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_WORLDSECTOR_H
#define BL_WORLDSECTOR_H

#include "BLPlane.h"
#include "BLArray.h"
#include "BLStreamFWD.h"
#include "BLMesh.h" // need a IResource definition

namespace mlabs { namespace balai {

namespace fileio {
	// world sector header - pack into 32B
	class AssetFile;
	struct WorldSectorAttribute {
		uint32	Name;		// name index to lookup name table or CRC if table N/A
		uint32  Splines;
		uint32	Cameras;
		uint32	Lights;
		uint32	Dummies;
		uint32	Materials; // only a reference, some platforms must duplicate some materials due to different fvf
		uint32	Meshes;
		uint32	Objects; // instances of mesh
	};
	BL_COMPILE_ASSERT(32==sizeof(WorldSectorAttribute), __worldsector_attribute_size_not_ok);
}

namespace math {
	class OBox3;
    class AABox3;
}

namespace graphics {

class WorldSector : public IResource
{
	BL_DECLARE_RTTI;

	// disable default/copy ctor and assignment operator
	BL_NO_DEFAULT_CTOR(WorldSector);
	BL_NO_COPY_ALLOW(WorldSector);

protected:
	// mesh list
	Array<IMesh*> meshes_;

	// material list
	Array<IMaterial*> materials_;
	
	struct Object {
		uint32 Name;
		uint32 MeshID;
		uint32 RPMask; // render pass mask
		uint16 MtlHead;
		uint16 MtlCount;
	};

private:
	struct BVHNode {
		sint32 Cell;	// index to both aabb and bisect plane array
		sint32 Front;	// if>0, index to bvhNode array, if<0, (-1-Front) index to leaf array
		sint32 Stride;	// as Front
		sint32 Back;	// as Front
	};

	struct BVHLeaf {
		uint32	ObjID;		// index to object array
		uint32	ObjCount;	// number of objects after ObjID
	};

	// private member
	math::Plane const* frustum_planes_; // this temporary holds 6 or more planes
	uint8*         visLeaves_;	 // size = numLeaves_ + numObjects_, visbility data(leave + objects), store frustum culling result, also the allocated memory pointer
	math::Matrix3* transforms_;  // size = numObjects_
	math::OBox3*   boxes_;		 // size = numObjects_
	Object*		   objects_;	 // size = numObjects_
	math::AABox3*  aabbs_;		 // size = numCells_, node's aabb array
	math::Plane*   planes_;      // size = numCells_, node's bisect plane array
	BVHNode*       bvhNodes_;	 // size = numNodes_
	BVHLeaf*       bvhLeaves_;	 // size = numLeaves_
	uint32*        mtlIDList_;   // size = unknown!!!!
	uint32         numObjects_;
	uint32         numCells_;
	uint32         numNodes_;
	uint32         numLeaves_;

	void Destroy_();

	// material and mesh chunk...
	virtual bool LoadMeshes_(fileio::AssetFile* file, fileio::ifstream& stream,
							 fileio::WorldSectorAttribute const& attr,
							 char const* strTable, uint32 strTableSize) = 0;
	//
	// to be removed...
	// render object
	virtual uint32 DrawObjects_(DrawMesh const* queue, uint32 totals, uint32 pass, math::Vector3 const& los) = 0;

	// test node
	void VisNodeTest_(sint32 node, uint32 planeState);

	// test leave
	void VisLeaveTest_(sint32 leave, uint32 planeState);

public:
	WorldSector(uint32 id, uint32 group);
	virtual ~WorldSector() { Destroy_(); }

	// visibility test
	bool VisbilityCulling(DrawMeshQueue& queue, math::Plane const* frustum_planes);

	// render(return objects drawn)
	uint32 DrawScene(uint32 pass, math::Vector3 const& los/*line of sight*/);

	// read from file stream...
	bool Read(fileio::AssetFile* file,
			  fileio::ifstream& stream,
			  fileio::WorldSectorAttribute const& attr);
};

//-----------------------------------------------------------------------------
class SceneManager
{
	Array<WorldSector*> worldSectors_; 
	
	// private
	SceneManager():worldSectors_(32) {}
	~SceneManager() {
		safe_release_functor func;
		worldSectors_.for_each(func);
		worldSectors_.clear();
	}

public:
	static SceneManager& GetInstance() {
		static SceneManager _inst;
		return _inst;
	}

	WorldSector* Find(uint32 name) const {
		uint32 const totals = worldSectors_.size();
		for (uint32 i=0; i<totals; ++i) {
			WorldSector* res = worldSectors_[i];
			if (res && name==res->Name()) {
				res->AddRef();
				return res;
			}
		}
		return NULL;
	}

	void ReleaseAll() {
		uint32 const totals = worldSectors_.size();
		for (uint32 i=0; i<totals; ++i) {
			BL_ASSERT(worldSectors_[i]);
			BL_ASSERT(1==worldSectors_[i]->RefCount());
			BL_SAFE_RELEASE(worldSectors_[i]);
		}
		worldSectors_.clear();
	}

	bool Register(WorldSector* res, bool force=false) {
		if (res) {
			WorldSector* res1 = force ? NULL:Find(res->Name());
			if (NULL==res1) {
				worldSectors_.push_back(res);
				return true;
			}
			else {
				res1->Release();
				return false;
			}
		}
		return false;
	}

	// render - return objects drawn
	uint32 DrawScene(uint32 pass, Camera const* cam);

	// visbility culling...(do once per frame)
	bool CollectDrawMeshes(DrawMeshQueue& queue, Camera const*);

	// factory
	static WorldSector* NewWorldSector(uint32 id, uint32 group);
	
	// name crc
	static uint32 HashName(char const* name) { return StringPool::HashString(name); }
};

}}} // namespace mlabs::balai::graphics

#endif // BL_WORLDSECTOR_H