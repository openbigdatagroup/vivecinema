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
#ifndef ML_MODEL_H
#define ML_MODEL_H

#include "BLFileStream.h"
#include "BLTransform.h"
#include "BLMaterial.h"
#include "BLSphere.h"

namespace mlabs { namespace balai {

namespace fileio {
//	class ResourceFile;
	// model header - pack into 32B
	struct ModelAttribute {
		uint32	Name;			// name index to lookup name table or CRC if table N/A
		float	Center[3];		// Bounding sphere center
		float	Radius;			// Bounding sphere radius
		uint32  MeshCount;		// mesh count
		uint32	AnimationCount;	// animation count
		uint32	MorphTargets;	// morph targets
	};
	BL_COMPILE_ASSERT(32==sizeof(ModelAttribute), __model_attribute_size_not_ok);
}

// lots of forward declarations
class Actor;
class AnimTrack;
class Animation;

namespace graphics {

class MeshInstance;
class DrawMeshQueue;

//-----------------------------------------------------------------------------
class Skeleton : public IShared 
{
	BL_DECLARE_RTTI;

	math::Transform* basePose_;		// initial pose(local transform)
	uint32*		jointNameHashes_;	// joint hash name array
	uint16*		parentIndices_;		// parent joint index array
	uint32		totalJoints_;		// total joints

	BL_NO_COPY_ALLOW(Skeleton);

	Skeleton():basePose_(NULL),jointNameHashes_(NULL),parentIndices_(NULL),totalJoints_(0) {}
	~Skeleton() {
		blFree(basePose_);
		basePose_		 = NULL;
		jointNameHashes_ = NULL;
		parentIndices_	 = NULL;
		totalJoints_	 = 0;
	}

	// if a hardpoint is updated, all decents children need to be updated 
	void UpdateDirtyJoints(math::Transform jointXforms[], math::Transform const localXforms[], uint32 jointId) const {
		BL_ASSERT(totalJoints_>0);
        BL_ASSERT(parentIndices_);
		for (uint32 i=jointId+1; i<totalJoints_; ++i) {
			if (parentIndices_[i]==jointId) {
				jointXforms[i].SetMul(jointXforms[jointId], localXforms[i]);
				UpdateDirtyJoints(jointXforms, localXforms, i); // resursive...
			}
		}
	}

	// frames animation update
	void UpdateJoints(math::Transform jointXforms[], math::Transform const& rootXform, math::Transform const localXforms[]) const {
		BL_ASSERT(jointXforms && localXforms);
		BL_ASSERT(totalJoints_>0);
		BL_ASSERT(parentIndices_);
		jointXforms[0].SetMul(rootXform, localXforms[0]);
		for (uint32 i=1; i<totalJoints_; ++i) {
			BL_ASSERT(parentIndices_[i]<i);
			jointXforms[i].SetMul(jointXforms[parentIndices_[i]], localXforms[i]);
		}
	}

	// initial pose
	void InitPose(math::Transform jointXforms[], math::Transform const& rootXform) const {
		jointXforms[0].SetMul(rootXform, basePose_[0]);
		for (uint32 i=1; i<totalJoints_; ++i) {
			BL_ASSERT(parentIndices_[i]<i);
			jointXforms[i].SetMul(jointXforms[parentIndices_[i]], basePose_[i]);
		}
	}

	// get index by name hash
	uint16 GetJointIndex(uint32 name) const {
		for (uint16 i=0; i<totalJoints_; ++i) {
			if (name==jointNameHashes_[i])
				return i;
		}
		return BL_BAD_UINT16_VALUE;
	}

	// get index by name hash
	uint16 GetParentJointIndex(uint32 name) const {
		for (uint16 i=0; i<totalJoints_; ++i) {
			if (name==jointNameHashes_[i])
				return parentIndices_[i];
		}
		return BL_BAD_UINT16_VALUE;
	}

	// V.I.P.
	friend class Model;
};

//
// -Models are not shared, Meshes are-
// To use class Model, implement class 'Has-A' class Model, see Actor for example.
//
class Model : public IObject
{	
	BL_DECLARE_RTTI;

	// mesh list
	Array<MeshInstance*> meshList_;

	// animations list
	Array<AnimTrack*> animList_;

	// render parameters and options
	DrawConfig      drawConfig_;

	// bounding sphere(wrt to jointXforms_[0])
	math::Sphere	boundingSphere_;

	// skeleton and transforms
	Skeleton*		 skeleton_;		// skeleton, backbone of this model
	math::Transform* jointXforms_;	// world joint transforms, update by skeleton, also used for rendering.
	math::Transform* animXforms_;	// local joint transforms, update by animation

	// blend shapes
	uint32*			blendShapes_;
	float*			blendWeights_;
	uint32			totalBlendShapes_;
	uint32			maxBlendShapes_;

	// name of this model - you don't change name, it's const!
	uint32 const	name_;	

	// clear
	void Clear_();

	// Clone - IObject interface
	IObject* DoClone_() const;

	// bind render data
	bool BindMesh_(MeshInstance*);

	// link animation
	bool LinkAnimTrack_(AnimTrack*);
	
	// no implementation!
	BL_NO_DEFAULT_CTOR(Model);
	BL_NO_COPY_ALLOW(Model);

public:
	explicit Model(uint32 name):IObject(),
		meshList_(4), // minimal alloc
		animList_(0), // empty
		drawConfig_(),
		boundingSphere_(),
		skeleton_(NULL),
		jointXforms_(NULL),
		animXforms_(NULL),
		blendShapes_(NULL),
		blendWeights_(NULL),
		totalBlendShapes_(0),
		maxBlendShapes_(0),
		name_(name) {}
	virtual ~Model() { Clear_(); }

	// name
	uint32 GetName() const { return name_; }

	// get bounding sphere in world space
	bool GetWorldBound(math::Sphere& sphere) const {
		if (jointXforms_) {
			sphere.SetTransform(jointXforms_[0], boundingSphere_);
			return !sphere.IsEmpty();
		}
		return false;
	}

	// create model from resource file
	bool CreateFromStream(fileio::ifstream& stream,
						  fileio::ModelAttribute const& attr,
						  char const* stringPool, uint32 stringPoolSize);
	// add/remove mesh
	bool AddMesh(IMesh* pMesh);
	bool RemoveMesh(IMesh* pMesh);

	// add/remove animation
	bool AddAnimation(Animation* anm, uint32 semantic);
	bool RemoveAnimation(Animation* anm);

	// get total joints(bone transform)
	uint32 GetTotalJoints() const {
		return (NULL!=skeleton_) ? (skeleton_->totalJoints_):0;
	}

	// get joint index by name
	uint16 GetJointIndex(uint32 name) const {
		return (NULL!=skeleton_) ? (skeleton_->GetJointIndex(name)):BL_BAD_UINT16_VALUE;
	}

	// get a joint's parent index
	uint16 GetParentJointIndex(uint32 name) const {
		return (NULL!=skeleton_) ? (skeleton_->GetParentJointIndex(name)):BL_BAD_UINT16_VALUE;
	}
	
	// get transform by known index(result from GetJointIndex/GetParentJointIndex) 
	math::Transform* GetJointTransformByIndex(uint32 id) {
		return (skeleton_ && id<skeleton_->totalJoints_) ? (jointXforms_+id):NULL;
	}
	math::Transform const* GetJointTransformByIndex(uint32 id) const {
		return (skeleton_ && id<skeleton_->totalJoints_) ? (jointXforms_+id):NULL;
	}

	// get transform by known index(result from GetJointIndex/GetParentJointIndex) 
	math::Transform const* GetJointTransform(uint32 name) const {
		if (skeleton_) {
			uint16 const id = skeleton_->GetJointIndex(name);
			if (id<skeleton_->totalJoints_)
				return (jointXforms_+id);
		}
		return NULL;
	}
	math::Transform const* GetParentJointTransform(uint32 name) const {
		if (skeleton_) {
			uint16 const id = skeleton_->GetParentJointIndex(name);
			if (id<skeleton_->totalJoints_)
				return (jointXforms_+id);
		}
		return NULL;
	}

	// update skeleton - for normal cases, do this one time for each frame
	void UpdateSkeleton(math::Transform const& Xform) {
		BL_ASSERT(skeleton_);

		// update hardpoints
		skeleton_->UpdateJoints(jointXforms_, Xform, animXforms_);
	}

	// update dirty skeleton - incase you modify a particular bone transform, and you need
	// to refresh all decents
	void UpdateDirtySkeleton(uint32 jointID) {
		BL_ASSERT(skeleton_);

		// update hardpoints
		skeleton_->UpdateDirtyJoints(jointXforms_, animXforms_, jointID);
	}

	// draw config

	// uv cell animation
	void SetTexCoordCell(uint32 frame) {
		if (frame)
			drawConfig_.flags |= RENDER_OPTION_TEXCELL_FRAME;
		else
			drawConfig_.flags &= ~RENDER_OPTION_TEXCELL_FRAME;
		drawConfig_.texCell = frame;
	}

	// init pose - you must call UpdateSkeleton() to skin... 
	// but, why do you need this?
//	void InitPose() {
//		BL_ASSERT(skeleton_);
//		std::memcpy(jointXforms_, skeleton_->initXforms_, skeleton_->totalJoints_*sizeof(Transform));
//	}

	// Pending... ponder display meshes, if a mesh is visible, push it into 'MeshRenderer'
	void CollectDrawMeshes(DrawMeshQueue& queue, uint32 param=0);

	// debug draw
	void DebugDraw();

	// vip
	friend class ::mlabs::balai::Actor;
};

//
// very candidate for prototype pattern?
//

// model library - a prototype manager
class ModelLibrary
{
	BL_NO_COPY_ALLOW(ModelLibrary);
	
	Array<Model*> modelList_;
	ModelLibrary():modelList_(128)
#ifdef BL_DEBUG_NAME
		,stringPool_(128, 2048)
#endif
	{}
	~ModelLibrary() { Clear(); }

public:
	static ModelLibrary& GetInstance() {
		static ModelLibrary inst_;
		return inst_;
	}
	void Clear() {
		uint32 const totals = modelList_.size();
		for (uint32 i=0; i<totals; ++i) {
			delete modelList_[i];
		}
		modelList_.clear();
	}
	Model const* Find(uint32 name) const {
		uint32 const totals = modelList_.size();
		for (uint32 i=0; i<totals; ++i) {
			if (name==modelList_[i]->GetName())
				return modelList_[i];
		}
		return NULL;
	}
	Model* BuildModel(uint32 name) const {
		uint32 const totals = modelList_.size();
		for (uint32 i=0; i<totals; ++i) {
			if (name==modelList_[i]->GetName())
				return (Model*) modelList_[i]->Clone();
		}
		return NULL;
	}
	Model* BuildModel(char const* name) const {
		return BuildModel(HashName(name));
	}
	bool AddModel(Model*& model) {
		if (model && NULL==Find(model->GetName())) {
			modelList_.push_back(model);
			model = NULL;
			return true;
		}
		return false;
	}
	Model* RemoveModel(uint32 name) {
		uint32 const totals = modelList_.size();
		for (uint32 i=0; i<totals; ++i) {
			Model* model = modelList_[i];
			if (name==model->GetName()) {
				modelList_.remove(i);
				return model;
			}
		}
		return NULL;
	}
	// model names - debug version only?
#ifdef BL_DEBUG_NAME
	StringPool stringPool_;
	void SaveName(char const* name) {
		stringPool_.AddString(name);
	}
	char const* FindName(Model* model) {
		return (NULL==model)? NULL:stringPool_.Find(model->GetName());
	}
#endif
	Model const* FindModel(char const* name) const {
		return (NULL!=name)? Find(StringPool::HashString(name)):NULL;
	}

	static uint32 HashName(char const* name) { return StringPool::HashString(name); }
};

#ifdef BL_DEBUG_NAME
#define SAVE_MODEL_NAME(name)	mlabs::balai::graphics::ModelLibrary::GetInstance().SaveName(name)
#define GET_MODEL_NAME(model)   mlabs::balai::graphics::ModelLibrary::GetInstance().FindName(model)
#else
#define SAVE_MODEL_NAME(name)
#define GET_MODEL_NAME(model)	(NULL)
#endif

}}} // namespace mlabs::balai::graphics

#endif // ML_MODEL_H