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
 * @file	BLActor.h
 * @author	andre chen
 * @history	2012/04/30 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_ACTOR_H
#define BL_ACTOR_H

#include "BLObject.h"
#include "BLTransform.h"
#include "BLSphere.h"

#include "BLAnimation.h" // PLAYMODE

namespace mlabs { namespace balai {

//
// Actor class contains a model and a simple animation blending scheme.
// it's impossible to implemnet an advance animation scheme to suite all
// game objects, all kinds of games.(or it's a waste for simple objects)
// Actor class implements a simple cross-fading animation and an additive
// animation blend all together.
//
namespace graphics { class Model; }
class AnimTrack;

class Actor : public IObject
{
	BL_DECLARE_RTTI;

	// no implement!
	BL_NO_COPY_ALLOW(Actor);

	// clone, to be overwritten...
	virtual IObject* DoClone_() const { return (blNew Actor(model_)); }

	// animation notifications
	virtual void OnAnimationRewind_(AnimTrack*)  {}
	virtual void OnAnimationEnd_(AnimTrack*) {}
	virtual void OnAnimationFade_(AnimTrack*, float /*fade*/) {}

	// update world bould(also update skeleton)
	void UpdateWorldBound_(bool rough=true) const;

private:
	math::Transform			xform_;
	mutable math::Sphere	worldBound_;
	graphics::Model*		model_;
	math::Transform*		pose_;			 // working buffer
	math::Transform*		pose2_;			 // blending buffer
	AnimTrack*				left_;			 // left branch animation, blend into right
	AnimTrack*				right_;			 // right branch animation, current state
	AnimTrack*				delta_;			 // additive blending
	float					animBlendWeight_;	// blending weight, (left, right) = (0.0f, 1.0)
	float					animBlendFactor_;	// blendweight +=  animBlendFactor_*frameTime;
	float					additiveWeight_; // additive blending weight
	float					additiveTimeFactor_;
	uint32					name_;

public:
	explicit Actor(uint32 name);
	explicit Actor(graphics::Model* model=NULL);
	virtual ~Actor() { Destroy(); name_=0; }

	// destroy
	void Destroy();

	// name - it may change the name
	uint32& Name() { return name_; }
	uint32  Name() const { return name_; }

	// model loaded?
	bool IsReady() const { return NULL!=model_; }

	// set/get transform(necessary?)
	void SetTransform(math::Transform const& xform) { xform_ = xform; }
	void SetPosition(float x, float y, float z) { xform_.SetOrigin(x, y, z); }
	math::Transform const& GetTransform() const { return xform_; }
	math::Vector3 const& GetPosition() const { return xform_.Origin(); }

	// get hardpoint index
	uint32 GetJointIndex(uint32 name) const;
	
	// get hardpoint(bone transform)
	math::Transform* GetJointTransformByIndex(uint32 id) const;

	// refresh dirty skeleton
	void UpdateDirtySkeleton(uint32 jointID);

	// get world bound
	math::Sphere const& GetWorldBound() const { return worldBound_; }

	// update skeleton, i.e. "skin"
	bool UpdateSkeleton() const {
		UpdateWorldBound_(true);
		return (NULL!=model_);
	}

	// customize mesh outlooks - not guarantee if meshes are designed to
	void SetTexCoordCell(uint32 frame) const;
	// more on...

	void DebugDraw() const;

	//////////////////////
	// animation
	void Animate(float deltaTime);
	uint32 GetCurrentAnimSemantic() const;
	uint32 GetAnimationCount() const;
	
	// set animtaion, instantly! no blending!
	void SetAnimationFadeOut(float fadeout) { if (fadeout>0.0f) animBlendFactor_=1.0f/fadeout; }
	bool SetAnimationBySemantic(uint32 semantic, PLAY_MODE mode);
	bool SetAnimationById(uint32 id, PLAY_MODE mode);
	uint32 GetAnimationSemanticById(uint32 id) const;
	uint32 GetAnimationIdBySemantic(uint32 semantic) const;

	// blend in new animation
	bool ChangeAnimationState(uint32 semantic, PLAY_MODE mode, float blendPeriod=0.25f);

	//
};

}} // namespace mlabs::balai

#endif // BL_ACTOR_H