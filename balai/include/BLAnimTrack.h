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
 * @file	BLAnimTrack.cpp
 * @author	andre chen
 * @history	2012/02/23 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_ANIMTRACK_H
#define BL_ANIMTRACK_H

#include "BLAnimation.h"

namespace mlabs { namespace balai {

// forward declarations
namespace graphics { class Model; }

// msg?
enum PLAYBACK {
	PLAYBACK_STOP   = 0,
	PLAYBACK_PLAY	= 1,
	PLAYBACK_REWIND	= 2, // notification(loop & ping-pong)
	PLAYBACK_END	= 3, // notification(one-time)
	PLAYBACK_FADE	= 4, // fade out(one-time)
	PLAYBACK_ERROR  = 5,

	// more msg?
};
typedef uint32 PLAYBACK_MSG;

// animation track
class AnimTrack : public IObject
{
	BL_DECLARE_RTTI;

	// input : animation
	Animation*		animation_;

	// animated joints and blendshape bind
	uint16*			refBind_;	// size = animation_->totalAnimatedJoints_ + animation_->totalMorphAnimations_;

	// playback speed and time
	float			speed_;
	float			time_;

	// semantic
	uint32 const	semantic_;

	// playback state - PLAYBACK_END, PLAYBACK_PLAY or PLAYBACK_ERROR
	PLAYBACK		playback_;

	// playback mode - PLAY_MODE_NORMAL, PLAY_MODE_REPEAT, PLAY_MODE_PING_PONG
	PLAY_MODE		playmode_;

	// IObject interface
	IObject* DoClone_() const { return (blNew AnimTrack(animation_, semantic_)); }

	// default constructor not define
	BL_NO_DEFAULT_CTOR(AnimTrack);
	BL_NO_COPY_ALLOW(AnimTrack);

public:
	AnimTrack(Animation* anm /* anim must be valid! */, uint32 semantic);
	~AnimTrack();

	// animation name
	uint32 GetName() const { return (NULL==animation_) ? BL_BAD_UINT32_VALUE:animation_->Name(); }

	// semantic
	uint32 Semantic() const { return semantic_; }

	// set play mode
	void SetPlayMode(PLAY_MODE mode) { playmode_ = mode; }
	
	// is additive
	bool IsAdditive() const { return (NULL!=animation_) && animation_->IsAdditive(); }

	// is playing
	bool IsPlaying() const { return PLAYBACK_PLAY==playback_; }

	// total animated joints
	uint32 NumJoints() const { return (NULL==animation_) ? 0:animation_->NumJoints(); }

	// joints' name list
	uint32 const* AnimJointNameList() const { return (NULL==animation_) ? NULL:animation_->jointNameHashArray_; }

	// total morphers
	uint32 NumBlendShapes() const { return (NULL==animation_) ? 0:animation_->numBlendShapes_; }

	// morphers' name list
	uint32 const* BlendShapeNameList() const { return (NULL==animation_) ? NULL:animation_->blendShapeNameHashArray_; }

	// accessors
	float Time() const		{ return time_; }
	float Duration() const	{ return (NULL!=animation_) ? animation_->duration_:0.0f; }
	void  Speed(float s)	{ speed_ = s; }
	float Speed() const		{ return speed_; }
	void  PlayAt(float t)	{ time_ = t; }
	void  Rewind() { time_ = 0.0f;	playback_ = PLAYBACK_PLAY; } // ready to play

	// debug used
#ifdef BL_DEBUG_BUILD
	float Frame() const { return (NULL!=animation_) ? (time_*animation_->sampleFrequency_):0.0f; }
#endif

	// movement
	bool Locomotion(math::Vector3& move, float elapsedTime) const {
		return (NULL!=animation_) && animation_->Locomotion(move, time_, time_+(speed_*elapsedTime), playmode_);
	}

	// time advance only
	PLAYBACK_MSG AdvanceTime(float elapsedTime, float fadeout=0.25f) {
		if (NULL!=animation_)  {
			BL_ASSERT(elapsedTime>=0.0f);
			BL_ASSERT(fadeout>=0.0f);
			if (PLAYBACK_PLAY==playback_) {
				float const prevTime = time_;
				float const dt = speed_ * elapsedTime;
				time_ = animation_->GetAnimTime(time_+dt, playmode_);
				float const duration = animation_->duration_;
				PLAYBACK_MSG msg = PLAYBACK_PLAY;
				if (PLAY_MODE_ONETIME==playmode_) {
					if (dt>0.0f) {
						if (prevTime<duration && time_>=duration)
							msg = playback_ = PLAYBACK_END;
						else if (((prevTime+fadeout)<duration) && ((time_+fadeout)>=duration))
							msg = PLAYBACK_FADE;
					}
					else {
						if (prevTime>0.0f && time_<=0.0f)
							msg = playback_ = PLAYBACK_END;
						else if ((prevTime>fadeout) && (time_<=fadeout))
							msg = PLAYBACK_FADE;
					}
				}
				else {
					if (dt>0.0f) {
						if (time_<prevTime)
							msg = PLAYBACK_REWIND;
					}
					else {
						if (time_>prevTime)
							msg = PLAYBACK_REWIND;
					}
				}

				return msg;
			}
			return playback_;
		}
		else {
			return playback_ = PLAYBACK_ERROR;
		}
	}

	// evaluate
	PLAYBACK_MSG Evaluate(math::Transform* xforms, float* weights, float elapsedTime, float fadeout=0.25f) {
		if (NULL!=animation_ && PLAYBACK_ERROR!=playback_) {
			float const prevTime = time_;
			float const dt = speed_ * elapsedTime;
			time_ = animation_->GetAnimTime(time_+dt, playmode_);
			float const duration = animation_->duration_;
			PLAYBACK_MSG msg = PLAYBACK_PLAY;
			if (PLAY_MODE_ONETIME==playmode_) {
				if (dt>0.0f) {
					if (((prevTime+fadeout)<duration) && ((time_+fadeout)>=duration)) {
						msg = PLAYBACK_FADE;
						// if elapsed time is too long, it may run through till the end
						if (prevTime<duration && time_>=duration)
							playback_ = PLAYBACK_END;
					}
					else if (prevTime<duration && time_>=duration)
						msg = playback_ = PLAYBACK_END;
				}
				else {
					if ((prevTime>fadeout) && (time_<=fadeout)) {
						msg = PLAYBACK_FADE;
						if (prevTime>0.0f && time_<=0.0f)
							playback_ = PLAYBACK_END;
					}
					else if (prevTime>0.0f && time_<=0.0f)
						msg = playback_ = PLAYBACK_END;
				}
			}
			else {
				if (dt>0.0f) {
					if (time_<prevTime)
						msg = PLAYBACK_REWIND;
				}
				else {
					if (time_>prevTime)
						msg = PLAYBACK_REWIND;
				}
			}
			
			float animTime = time_;
			if (PLAY_MODE_PING_PONG==playmode_ && animTime>animation_->duration_) {
				animTime = (2.0f*animation_->duration_) - animTime;
				BL_ASSERT(animTime>=0.0f);
			}

			// eval skin
			animation_->EvalJointXforms(animTime, xforms, refBind_);

			// eval blend shape(not ready!!!)
			if (animation_->numBlendShapes_ && weights)
				animation_->EvalBlendShapes(animTime, weights, refBind_+animation_->NumJoints());

			return msg;
		}
		else {
			return playback_ = PLAYBACK_ERROR;
		}
	}

	// vip
	friend class graphics::Model;
};
//BL_COMPILE_ASSERT(32==sizeof(AnimTrack), AnimTrack_size_is_not32);

}} // namespace mlabs::balai

#endif // !defined(BL_ANIMTRACK_H)