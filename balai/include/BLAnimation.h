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
 * @file	BLAnimation.h
 * @author	andre chen
 * @history	2012/02/23 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_ANIMATION_H
#define BL_ANIMATION_H

#include "BLResourceManager.h"
#include "BLTransform.h"
#include "BLStringPool.h"
#include "BLObject.h"
#include "BLStreamFWD.h"

namespace mlabs { namespace balai { namespace fileio {
	// animation set header
	struct AnimationSetAttribute {
		uint32	Name;					// AnimationSet name, index to lookup name table or CRC if table N/A
		uint32  NumSamples;				// total frames
		uint32	JointKeyOffset;			// data offset to first joint transform
		uint32	NumBlendShapeAlphas;	// total morph animation keys(accumulated)
		uint32	BlendShapeReserve1;
		uint32	BlendShapeReserve2;
		uint16	NumLerpJoints;			// 2 keys interpolation animation
		uint16	NumSampleJoints;		// sample animation
		uint8   NumBlendShapes;			// total morph animations
		uint8   SampleFrequency;		// frame per second
		uint8	FootprintKeySize;		// sizeof(Vector3) = 12, or 0 if n/a
		uint8	Additive;				// true or false
	};
	BL_COMPILE_ASSERT(32==sizeof(AnimationSetAttribute), AnimationSetAttribute_size_is_not_32);
}

// play mode
enum PLAY_MODE {
	PLAY_MODE_ONETIME	= 0,	// one time
	PLAY_MODE_REPEAT	= 1,	// repeat
	PLAY_MODE_PING_PONG = 2,	// back and forth
};

namespace anim_helper {
template<typename T>
inline float find_between_keys(T const* time, uint32& lo, uint32& hi, float frame, uint32 const last_frame, uint32 time_base)
{
	float alpha = 0.0f;
	uint32 const frame_base = lo;
	uint32 const total_keys = hi - lo;
	hi -= 1; // the last index
	BL_ASSERT(hi>lo);
	if (0xffffffff==time_base) {	
		BL_ASSERT(2==total_keys || (last_frame+1)==total_keys);
		if ((last_frame+1)==total_keys) {
			lo = (uint32) frame;
			if (lo<last_frame) {
				hi = lo + 1;
				alpha = frame - lo;
			}
			else {
				lo = hi = last_frame;
				alpha = 0.0f;
			}
			lo += frame_base;
			hi += frame_base;
		}
		else {
			alpha = frame/last_frame;
		}
	}
	else {
		uint32 m;
		lo = time_base;
		hi = lo + total_keys - 1;
		while ((lo+1)<hi) {
			m = (lo+hi)>>1;
			if (frame<(float) time[m])
				hi = m;
			else
				lo = m;
		}

		alpha = (frame-(float)time[lo])/((float)time[hi] - (float)time[lo]);	
		
		lo = frame_base + lo - time_base;
		hi = frame_base + hi - time_base;
	}
	return alpha;
}
} // namespace anim_helper

class Animation : public IResource
{
	BL_DECLARE_RTTI;

	// disable default/copy ctor and assignment operator
    BL_NO_DEFAULT_CTOR(Animation);
	BL_NO_COPY_ALLOW(Animation);

	// constants
	enum {
		// attribute
		ATTRIBUTE_ADDITIVE	= 1,	// this animation is for additive blending
	};

	// skinning
	math::Transform*	jointKeyArray_;		// size = numJointKeys_
	uint32*		jointNameHashArray_;		// size = numJoints_
	
	// blendshapes
	float*		blendShapeAlphaArray_;		// size = numBlendShapes_
	uint8*		blendShapeTimeArray_;		// point to either uint8, uint16 or uint32, size = numBlendShapes_
	uint32*		blendShapeNameHashArray_;	// size = numBlendShapes_
	uint32*		blendShapeOffsetArray_;		// size = numBlendShapes_
	
	// drive - footstep track, footstep start from (0,0,0)
	math::Vector3*	tracks_;			// size = totalFrames_ + 2 if not NULL
										//	tracks_[0], tracks_[1] = inf, sup respectively.
										//  foot track start from tracks_[2] = (0,0,0) to tracks_[totalFrames_+2]
	// counts and properties
	float		duration_;				// duration_ = (totalFrames_ - 1)/sampleFrequency_
	float		sampleFrequency_;		// frames per seconds(Hz)
	uint32		numSamples_;			// total frames
	uint32		numBlendShapes_;		// TO-DO : blendshape
	uint32		numBlendShapeAlphas_;	// TO-DO : blendshape
	uint32		attribute_;				// attribute, is additive?
	uint16		numInterpJoints_;		// simplified animation, 2 keyframes interpolation
	uint16		numSampleJoints_;		// animation with all keyframes sampled
	
	// dtor
	virtual ~Animation() { Destroy(); }

#if 0
	// private calls - do not call me!
	template<typename T>
	void eval_joint_xforms_(T const* time, float frame, Transform* xforms, uint16* indices) const {
		BL_ASSERT(numJoints_);
		uint32 const last_frame = numFrames_ - 1;
		uint32 const last_anim  = numJoints_ - 1;

		uint32 lo(0), hi(0);
		float alpha = 0.0f;
		uint32 id = 0;
		for (uint32 i=0; i<last_anim; ++i) {
			id = indices[i];
			if (id!=BL_BAD_UINT16_VALUE) {
				lo = JointKeyOffsetArray_[i];
				hi = JointKeyOffsetArray_[i+1];

				alpha = anim_helper::find_between_keys(time, lo, hi, frame, last_frame, JointTimeOffsetArray_[i]);
				xforms[id].SetLerp(jointKeyArray_[lo], jointKeyArray_[hi], alpha);
			}
		}

		// last
		id = indices[last_anim];
		if (id!=BL_BAD_UINT16_VALUE) {
			lo = JointKeyOffsetArray_[last_anim];
			hi = numJointKeys_;

			alpha = anim_helper::find_between_keys(time, lo, hi, frame, last_frame, JointTimeOffsetArray_[last_anim]);
			xforms[id].SetLerp(jointKeyArray_[lo], jointKeyArray_[hi], alpha);
		}
	}

	template<typename T>
	void Blend_timekey_(T const* time, float frame, Transform** joints, float** morphs, float lerp) const {
		uint32 lo(0), hi(0), base(0);

		// frame
		uint32 const last_frame = numFrames_ - 1;
		float alpha = 0.0f;

		// skinning transforms
		Transform t;
		if (0==(ATTRIBUTE_ADDITIVE&attribute_)) {
			for (uint32 i=0; i<numJoints_; ++i) {
				lo = JointKeyOffsetArray_[i];
				hi = (i+1)<numJoints_ ? JointKeyOffsetArray_[i+1]:numJointKeys_; // last!
				alpha = anim_helper::find_between_keys(time, lo, hi, frame, last_frame, JointTimeOffsetArray_[i]);
				t.SetLerp(jointKeyArray_[lo], jointKeyArray_[hi], alpha);
				joints[i]->SetLerp(*joints[i], t, lerp);
			}
		}
		else {
			Transform diff;
			for (uint32 i=0; i<numJoints_; ++i) {
				base = lo = JointKeyOffsetArray_[i];
				hi = (i+1)<numJoints_ ? JointKeyOffsetArray_[i+1]:numJointKeys_;
				alpha = anim_helper::find_between_keys(time, lo, hi, frame, last_frame, JointTimeOffsetArray_[i]);

				if (base!=lo) {
					diff.SetLerp(jointKeyArray_[lo], jointKeyArray_[hi], alpha);
				}
				else {
					// first frame
					diff.SetLerp(Transform(), jointKeyArray_[hi], alpha);
				}
				t = diff*jointKeyArray_[base];
				joints[i]->SetLerp(*joints[i], t, lerp);
			}
		}

		// morph alphas
//		float t2;
		for (uint32 i=0; i<numBlendShapes_; ++i) {
//			lo = morphAnimOffsetArray_[i];
//			hi = (i+1)<totalMorphAnimations_ ? morphAnimOffsetArray_[i+1]:totalMorphAnimKeys_; // last!
//			hi -= 1;
//			BL_ASSERT(hi>=lo);
//			while ((lo+1)<hi) {
//				m = (lo+hi)>>1;
//				if (morphAnimTimeArray_[m]>time)
//					hi = m;
//				else
//					lo = m;
//			}
//			alpha = (time-morphAnimTimeArray_[lo])/(morphAnimTimeArray_[hi]-morphAnimTimeArray_[lo]);
//			t2 = (1.0f-alpha)*morphAnimKeyArray_[lo] + alpha*morphAnimKeyArray_[hi];
//			*morphs[i] = lerp*t2 + (1.0f-lerp)*(*morphs[i]);
			*morphs[i] = 0.0f;
		}
	}

	template<typename T>
	void Additive_timekey_(T const* time, float frame, Transform** joints, float** morphs, float weight) const {
		if (weight<0.001f)
			return;

		uint32 lo(0), hi(0), base(0);

		// frame
		uint32 const last_frame = numFrames_ - 1;
		float alpha = 0.0f;

		// skinning transforms
		Transform t;
		if (0==(ATTRIBUTE_ADDITIVE&attribute_)) {
			for (uint32 i=0; i<numJoints_; ++i) {
				lo = JointKeyOffsetArray_[i];
				hi = (i+1)<numJoints_ ? JointKeyOffsetArray_[i+1]:numJointKeys_; // last!
				alpha = anim_helper::find_between_keys(time, lo, hi, frame, last_frame, JointTimeOffsetArray_[i]);
				t.SetLerp(jointKeyArray_[lo], jointKeyArray_[hi], alpha);
				joints[i]->SetAddBlend(*joints[i], t, weight);
			}
		}
		else {
			for (uint32 i=0; i<numJoints_; ++i) {
				base = lo = JointKeyOffsetArray_[i];
				hi = (i+1)<numJoints_ ? JointKeyOffsetArray_[i+1]:numJointKeys_; // last!
				alpha = anim_helper::find_between_keys(time, lo, hi, frame, last_frame, JointTimeOffsetArray_[i]);
				if (base==lo) {
					t.SetLerp(Transform(), jointKeyArray_[hi], alpha);
				}
				else {
					t.SetLerp(jointKeyArray_[lo], jointKeyArray_[hi], alpha);
				}
				
				joints[i]->SetLerp(*joints[i], t*(*joints[i]), weight);
			}
		}

		// morph alphas
		for (uint32 i=0; i<numBlendShapes_; ++i) {
//			lo = morphAnimOffsetArray_[i];
//			hi = (i+1)<totalMorphAnimations_ ? morphAnimOffsetArray_[i+1]:totalMorphAnimKeys_; // last!
//			hi -= 1;
//			BL_ASSERT(hi>=lo);
//			while ((lo+1)<hi) {
//				m = (lo+hi)>>1;
//				if (morphAnimTimeArray_[m]>time)
//					hi = m;
//				else
//					lo = m;
//			}
//			alpha = (time-morphAnimTimeArray_[lo])/(morphAnimTimeArray_[hi]-morphAnimTimeArray_[lo]);
//			*morphs[i] += weight*((1.0f-alpha)*morphAnimKeyArray_[lo] + alpha*morphAnimKeyArray_[hi]);
			*morphs[i] = 0.0f;
		}
	}
#endif

public:
	Animation(uint32 name, uint32 group):IResource(name, group),
		jointKeyArray_(NULL),jointNameHashArray_(NULL),
		blendShapeAlphaArray_(NULL),blendShapeTimeArray_(NULL),blendShapeNameHashArray_(NULL),blendShapeOffsetArray_(NULL),
		tracks_(NULL),
		duration_(0.0f),sampleFrequency_(0.0f),numSamples_(0),
		numBlendShapes_(0),numBlendShapeAlphas_(0),
		attribute_(0),
		numInterpJoints_(0),numSampleJoints_(0) {}
	void Destroy() {
		if (jointKeyArray_) {
			blFree(jointKeyArray_);
			jointKeyArray_ = NULL;
		}
		jointNameHashArray_	  = NULL;
		blendShapeAlphaArray_ = NULL;
		blendShapeTimeArray_  = NULL;
		blendShapeNameHashArray_ = NULL;
		blendShapeOffsetArray_ = NULL;
		tracks_				  = NULL;
		numBlendShapes_		  = 0;
		numBlendShapeAlphas_  = 0;
		numSamples_			  = 0;
		sampleFrequency_	  = 0.0f;
		duration_			  = 0.0f;
		attribute_			  = 0;

		numInterpJoints_ = 0;
		numSampleJoints_ = 0;
	}

	// create from stream...
	bool CreateFromStream(fileio::ifstream&, fileio::AnimationSetAttribute const&);

	// additive?
	bool IsAdditive() const { return 0!=(ATTRIBUTE_ADDITIVE&attribute_); }

	// total joints
	uint32 NumJoints() const { return (numInterpJoints_+numSampleJoints_); }

	// animation drive
	bool MoveRange(math::Vector3& inf, math::Vector3& sup) const {
		if (tracks_) {
			inf = tracks_[0];
			sup = tracks_[1];
			return true;
		}
		return false;
	}
	bool Movement(math::Vector3& offset) const {
		if (tracks_) {
			offset =  tracks_[numSamples_+1]; //  tracks_[2] = {0,0,0};
			return true;
		}
		return false;
	}
	bool Locomotion(math::Vector3& move, float curTime, float nextTime, PLAY_MODE mode) const;

	// get animation playback time, if ping-pong, the time may be bigger than duration_
	//  parameters :
	//			curTime   : current time
	//			deltaTime : time advance
	//			mode      = playing mode : one-time, repeat or ping-pong
	//
	float GetAnimTime(float time, PLAY_MODE mode) const {
		BL_ASSERT(duration_>=0.0f);

		// normal case
		if (0.0f<=time && time<=duration_)
			return time;

		//
		// time<0 or time>duration_
		//
		if (PLAY_MODE_REPEAT==mode) {
			if (time>duration_)
				time = math::FMod(time, duration_);
			else
				time = duration_; // ok for 99.9999%(or we are at very low frame rate!)
		}
		else if (PLAY_MODE_PING_PONG==mode) {
			if (time<0.0f)
				time = -time; // symmetric

			float const cycle = 2.0f*duration_;
			if (time>=cycle)
				time = math::FMod(time, cycle); // caution : this may larger than duration_
		}
		else {
			// normal case, 1 time play
			if (time>duration_)
				time = duration_;
			else
				time = 0.0f;
		}

		return time;
	}

	void EvalJointXforms(float animTime, math::Transform* xforms, uint16* indices) const {
		BL_ASSERT(0.0f<=animTime && animTime<=duration_);
		if (numInterpJoints_) {
			float alpha = animTime/duration_;
			for (uint32 i=0; i<numInterpJoints_; ++i) {
				uint32 const id = indices[i];
				if (BL_BAD_UINT16_VALUE!=id) {
					xforms[id].SetLerp(jointKeyArray_[i], jointKeyArray_[numInterpJoints_+i], alpha);
				}
			}
		}
		if (numSampleJoints_) {
			float const frame = animTime * sampleFrequency_;
			BL_ASSERT(frame<=numSamples_);
			int const l = (int)frame;
			float const alpha = frame - l;
			math::Transform const* keyframe1 = jointKeyArray_ + 2*numInterpJoints_ + l*numSampleJoints_;
			math::Transform const* keyframe2 = keyframe1 + numSampleJoints_;
			for (uint32 i=0; i<numSampleJoints_; ++i) {
				uint32 const id = indices[numInterpJoints_+i];
				if (BL_BAD_UINT16_VALUE!=id) {
					xforms[id].SetLerp(keyframe1[i], keyframe2[i], alpha);
				}
			}
		}
	}

	//
	// TO-DO...
	//
	void EvalBlendShapes(float /*animTime*/, float* /*weights*/, uint16* /*indices*/) const {
	}

	// vip - the instance
	friend class AnimTrack;
};

//-----------------------------------------------------------------------------
// Animation manager
class AnimationManager : public ResourceManager<Animation, 128>
{
	// private
	AnimationManager()
#if defined(BL_DEBUG_NAME) || defined(BL_DEBUG_ANIMATION_NAME)
		:stringPool_()
#endif
	{}
	~AnimationManager() {}

public:
	static AnimationManager& GetInstance() {
		static AnimationManager _inst;
		return _inst;
	}

	// animation names - debug version only?
#if defined(BL_DEBUG_NAME) || defined(BL_DEBUG_ANIMATION_NAME)
	StringPool stringPool_;
	void SaveName(char const* name) {
		stringPool_.AddString(name);
	}
	char const* FindName(Animation const* anim) {
		return (NULL==anim)? NULL:stringPool_.Find(anim->Name());
	}
	char const* TransName(uint32 name) {
		return stringPool_.Find(name);
	}
#endif
	Animation* FindAnimation(char const* name) const {
		return (NULL!=name)? Find(HashName(name)):NULL;
	}

	static uint32 HashName(char const* name) { return StringPool::HashString(name); }
};

}} // namespace mlabs::balai

#if defined(BL_DEBUG_NAME) || defined(BL_DEBUG_ANIMATION_NAME)
#define SAVE_ANIMATION_NAME(name)		mlabs::balai::AnimationManager::GetInstance().SaveName(name)
#define GET_ANIMATION_NAME(anim)		mlabs::balai::AnimationManager::GetInstance().FindName(anim)
#define TRANSLATE_ANIMATION_NAME(name)	mlabs::balai::AnimationManager::GetInstance().TransName(name)
#else
#define SAVE_ANIMATION_NAME(name)
#define GET_ANIMATION_NAME(anim)		(NULL)
#define TRANSLATE_ANIMATION_NAME(name)  (NULL)
#endif

#endif // !defined(BL_ANIMATION_H)
