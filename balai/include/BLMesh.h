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
 * @file	BLMesh.h
 * @author	andre chen
 * @history	2012/02/20 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_MESH_H
#define BL_MESH_H

#include "BLMaterial.h" // include "BLGraphics.h" >> "BLShader.h" >> "BLResourceManager.h" >> "BLStringPool.h"
#include "BLSphere.h"
#include "BLVertexFormat.h"

// mesh header
namespace mlabs { namespace balai { namespace fileio {
	// Mesh header - pack into 32B
	struct MeshAttribute {
		uint32	Name;				// name index to lookup name table or CRC if table N/A
		uint32	FVF;				// FVF : vertex format code
		uint32	VertexDataOffset;	// offset to vertex data, 32B aligned
		uint32	FaceDataOffset;		// offset to index data, 32B aligned
		uint32	BoneDataSize;		// bone table size
		uint16	TotalMaterials;		// total materials this mesh has
		uint16	TotalBatches;		// total batches
		uint16	TotalVertices;		// at most 65535 vertices and yes, it must use 16bits index
		uint16	TotalFaces;			// total faces
		uint8	TotalBones;			// since we will use 'uint8' for BoneIDs, max bones = 255
		uint8	MaxVertexInfls;		// 0, 1, 2, 3 or 4
		uint8	TotalVertexAttrs;	// # of vertex attribute, must>=1(must have position)
		uint8	VertexStride;		// size(in bytes) of a vertex
	};
	BL_COMPILE_ASSERT(32==sizeof(MeshAttribute), MeshAttribute_is_not_32);
}}}

namespace mlabs { namespace balai { namespace graphics {

struct SkinInfo {
	uint8 Indices[4];	// bone indices
	uint8 Weights[4];	// bone weights
};

// mesh
class IMesh : public IResource
{
	BL_DECLARE_RTTI;

	// disable default/copy ctor and assignment operator
	BL_NO_DEFAULT_CTOR(IMesh);
	BL_NO_COPY_ALLOW(IMesh);

protected:
	math::Sphere	boundingSphere_;
	math::Matrix3 const* invBindPoseXforms_;	// aka bone offset matrices
	uint32*			boneNames_;			// bone names
	uint32*			blendShapeNames_;	// blend shape names
	uint32			totalFaces_;		// total faces
	uint32			totalVertices_;		// # vertices < 65536(INDEX16)
	uint32			rpMask_;		    // render pass mask, collection of all materials' passes.
	uint16			totalBones_;		// < 255
	uint16			totalBlendShapes_;	// < VERTEX_ATTRIBUTE_MORPH_TARGET_LIMIT = 8,

	IMesh(uint32 id, uint32 group):IResource(id, group),
		boundingSphere_(),
		invBindPoseXforms_(NULL),
		boneNames_(NULL),
		blendShapeNames_(NULL),
		totalFaces_(0),
		totalVertices_(0),
		rpMask_(0),
		totalBones_(0),
		totalBlendShapes_(0) {}

	// non-public dtor - use Release() instead!
	virtual ~IMesh() { 
		boundingSphere_.Reset();
		invBindPoseXforms_ = NULL;
		boneNames_		   = NULL;
		blendShapeNames_   = NULL;
		totalFaces_		   = 0;
		totalVertices_	   = 0;
		rpMask_		       = 0;
		totalBones_		   = 0;
		totalBlendShapes_  = 0;
	}

public:
	math::Sphere const& GetLocalBound() const { return boundingSphere_; }
	math::Matrix3 const* BoneOffsetMatrixArray() const { return invBindPoseXforms_; }
	uint32 const* BoneNameArray() const		{ return boneNames_; }
	uint32 const* BlendShapeNameArray() const { return blendShapeNames_; }
	uint32 TotalBones() const               { return totalBones_; }
	uint32 TotalBlendShapes() const         { return totalBlendShapes_; }
	uint32 TotalVertices() const            { return totalVertices_; }
	uint32 TotalFaces() const				{ return totalFaces_; }

	// render mesh wrt pass specified. this is the simple(and probably wrong) way to
	// draw a mesh. but if it draws multiple meshes one after another this way, 
	// the result might probably be wrong. consider the alpha blending pass,
	// drawing all meshes from far to near might be required.
	virtual bool Render(math::Matrix3 const*, float const*, DrawConfig const*, uint32 pass) = 0;

	// sequential rendering
	virtual uint32 BeginDraw(uint32 pass) const = 0;
	virtual bool DrawSubset(uint32 subset, math::Matrix3 const*, float const*, DrawConfig const*, uint32 pass, bool applyMtl=true) const = 0;
	virtual void EndDraw() const {}

// UGLY Direct3D9 device lost handlers	
#ifdef BL_RENDERER_D3D
	virtual bool OnCreateDevice()	{ return true; }
	virtual bool OnResetDevice()	{ return true; }
	virtual bool OnLostDevice()		{ return true; }
	virtual bool OnDestroyDevice()  { return true; }
#endif
};

//- mesh instance(hold by a model) --------------------------------------------
class MeshInstance
{
	IMesh*                  mesh_;         // Mesh
	math::Matrix3*          xforms_;       // matrix use for rendering, each mesh instance hold a copy of render matrics to submit to GPU(shader hardware)
	math::Transform const**	bones_;        // joint ptr array, point to joints hold by model
	float const**           blendShapes_;  // blend Shapes weight ptr array, point to alpha hold by model
	float*                  blendWeights_; // blend weights for rendering.

	// default constructor and assignment operator not define(use Clone)
	BL_NO_DEFAULT_CTOR(MeshInstance);
	BL_NO_COPY_ALLOW(MeshInstance);

public:
	explicit MeshInstance(IMesh* mesh):
		mesh_(mesh),xforms_(NULL),bones_(NULL),blendShapes_(NULL),blendWeights_(NULL) { 
		BL_COMPILE_ASSERT((sizeof(void const*)==sizeof(math::Transform const*)), bad_transform_const_ptr_size);
		BL_COMPILE_ASSERT((sizeof(void const*)==sizeof(float const*)), bad_float_const_ptr_size);
		BL_ASSERT(mesh_!=NULL);
		if (NULL!=mesh_) {
			mesh_->AddRef();
			uint32 const bones = mesh_->TotalBones();
			uint32 const blendShapes = mesh_->TotalBlendShapes();
			uint32 const required_size = (bones*sizeof(math::Matrix3)) + (bones*sizeof(math::Transform const*)) + 
										 (blendShapes*sizeof(float const*)) + (BL_ALIGN_UP(blendShapes, 4)*sizeof(float));
			uint8* buf = (uint8*) blMalloc(required_size);
			std::memset(buf, 0, required_size); // out of memory!?
			xforms_ = (math::Matrix3*) buf; 
			buf += (bones*sizeof(math::Matrix3));
			bones_ = (math::Transform const**) buf;
			if (blendShapes>0) {
				buf += (bones*sizeof(math::Transform const*));
				blendShapes_ = (float const**) buf;
				buf += (blendShapes*sizeof(float*));
				blendWeights_ = (float*) buf;
			}
		}
	}
	~MeshInstance() {
		BL_SAFE_RELEASE(mesh_);
		blFree(xforms_);
		xforms_		  = NULL;
		bones_        = NULL;
		blendShapes_  = NULL;
		blendWeights_ = NULL;
	}
	MeshInstance* Clone() const {
		return (blNew MeshInstance(mesh_));
	}

	// get world bounding sphere
	void GetWorldBound(math::Sphere& s) const {
		s.SetTransform(xforms_[0], mesh_->GetLocalBound());
	}

	// call by MeshRenderer when this mesh push into queue, may do software morphing/skinning...
	// update transforms and blend weights
	bool UpdateTransformsAndBlendWeights() {
		if (NULL!=mesh_) {
			uint32 const totalBones = mesh_->TotalBones();
			math::Matrix3 const* invBindPoseXforms = mesh_->BoneOffsetMatrixArray();
			for (uint32 i=0; i<totalBones; ++i) {
				xforms_[i].SetTransform(*(bones_[i]));
				xforms_[i] *= invBindPoseXforms[i];
			}
			uint32 const totalMorphers = mesh_->TotalBlendShapes();
			if (totalMorphers) {
				BL_ASSERT(blendWeights_);
				uint32 const fillAlphas = BL_ALIGN_UP(totalMorphers, 4);
				BL_ASSERT(fillAlphas<=VERTEX_ATTRIBUTE_BLENDSHAPE_LIMIT);
				for (uint32 i=0; i<fillAlphas; ++i) {
					blendWeights_[i] = i<totalMorphers ? *(blendShapes_[i]):0.0f;
				}
			}
			return true;
		}
		return false;
	}

	// sort by materials?
	void Render(DrawConfig const& drawConfig, uint32 pass) {
		if (mesh_) {
	 	    mesh_->Render(xforms_, blendWeights_, &drawConfig, pass);
		}
	}

	// vip
	friend class Model;
};

//- a draw mesh... collected by DrawMeshQueue ---------------------------------
struct DrawMesh {
	IMesh*               mesh;
	math::Matrix3 const* transforms;
	float const*         blendWeights;
    DrawConfig const*    config;
	float                distance;  // distance from camera.
    uint32               mtlID;     // begin material index, if invalid value, mesh's materials are taken.
	uint32				 rpMask;    // render pass mask, this could be different to mesh's render pass mask,
	                                // occur when different "material ball" apply to the same mesh.
	uint32               param;    // special parameters and pad to 32B
	DrawMesh():mesh(NULL),transforms(NULL),blendWeights(NULL),config(NULL),
	distance(0.0f),mtlID(0),rpMask(0),param(0) {}
};
//BL_COMPILE_ASSERT(32==sizeof(DrawMesh), unexpected_DrawMesh_size);

//---------------------------------------------------------------------------
class DrawMeshQueue
{
	Array<IMaterial*> materials_;
	Array<DrawMesh>   drawLists_;

public:
	// if we does not pre-alloc some memory here, a Finalize() is need to
	// ensure release memory before MemoryManager R.I.P.(if DrawMeshQueue is
	// embed in some static object, e.g. MyApp class). But if we do reverve some
	// memory in this ctor, a memory bouble may be created as well)
	DrawMeshQueue():materials_(),drawLists_() {}
	~DrawMeshQueue() {}
	
	// reserve mesh draw list(call it multiple time!)
	void Reserve(uint32 maxMeshes, uint32 maxMaterials=0) {
		drawLists_.reserve(maxMeshes);
		if (maxMaterials)
		    materials_.reserve(maxMaterials);
	}
	void Clear() {
		materials_.clear();
		drawLists_.clear();
	}

	// this call is critical for releasing memory before MemoryManager R.I.P.
	void Finalize() { // free memory
		materials_.cleanup();
		drawLists_.cleanup();
	}

	uint32 TotalMaterials() const { return materials_.size(); }
	uint32 Size() const { return drawLists_.size(); }

	uint32 Push(IMaterial* mtl) {
		BL_ASSERT(NULL!=mtl);
		if (mtl) {
			uint32 const totals = materials_.size();
			if ((totals+1)==materials_.push_back(mtl))
				return totals;
		}
		return BL_BAD_UINT32_VALUE;
	}

	uint32 Push(DrawMesh const& d) {
		BL_ASSERT(NULL!=d.mesh && NULL!=d.transforms);
		if (NULL!=d.mesh && NULL!=d.transforms) {
			uint32 const totals = drawLists_.size();
			if ((totals+1)==drawLists_.push_back(d))
				return totals;
		}
		return BL_BAD_UINT32_VALUE;
	}

	IMaterial const* GetMaterial(uint32 id) const {
		return (id<materials_.size()) ? materials_[id]:NULL;
	}

	DrawMesh const& operator[](uint32 id) const {
		return drawLists_[id];
	}
};

//- mesh manager --------------------------------------------------------------
class MeshManager : public ResourceManager<IMesh, 256>
{
	// private
	MeshManager()
#ifdef BL_DEBUG_NAME
		:stringPool_(256, 4096)
#endif
	{}
	~MeshManager() {}

public:
	static MeshManager& GetInstance() {
		static MeshManager _inst;
		return _inst;
	}

	// mesh names - debug version only?
#ifdef BL_DEBUG_NAME
	StringPool stringPool_;
	void SaveName(char const* name) {
		stringPool_.AddString(name);
	}
	char const* FindName(Mesh const* mesh) {
		return (NULL==mesh)? NULL:stringPool_.Find(mesh->Name());
	}
#endif
	IMesh* FindMesh(char const* name) const {
		return (NULL!=name)? Find(HashName(name)):NULL;
	}

	// crc name
	static uint32 HashName(char const* name) { return StringPool::HashString(name); }
};

}}} // namespace mlabs::balai::graphics

#ifdef BL_DEBUG_NAME
#define SAVE_MESH_NAME(name) mlabs::balai::graphics::MeshManager::GetInstance().SaveName(name)
#define GET_MESH_NAME(mesh)  mlabs::balai::graphics::MeshManager::GetInstance().FindName(mesh)
#else
#define SAVE_MESH_NAME(name)
#define GET_MESH_NAME(mesh)	(NULL)
#endif

#endif // ML_MESH_H