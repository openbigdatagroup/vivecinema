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
 * @file	BLParticle.h
 * @author	andre chen
 * @history	2012/05/21 created
 */
#ifndef BL_PARTICLE_H
#define BL_PARTICLE_H

#include "BLTexture.h"
#include "BLVector3.h"
#include "BLColor.h"
#include "BLArray.h"

namespace mlabs { namespace balai {

struct Particle
{
	math::Vector3	pos; // position
	math::Vector3	vel; // velocity
	graphics::Color	color;
	float   life;
	float   age;
	float   size;
	Particle* next;
};

typedef bool (*ParticleUpdateCallback)(Particle* particle, float elapsedTime);

struct safe_blfree_functor {
	template<typename T>
	bool operator()(T*& ptr) const {
		blFree(ptr);
		ptr = NULL;
		return true;
	}
};

class ParticleTank
{
	enum { PARTICLE_BATCH = 256 };
	Array<Particle*> alloc_;
	Particle*  activeList_;
	Particle*  freeList_;
	
	void RecycleParticle_(Particle* p) {
		if (p) {
			p->next = freeList_;
			freeList_ = p;
		}
	}

public:
	ParticleTank():alloc_(128),activeList_(NULL),freeList_(NULL) {}
	~ParticleTank() {
		// prevent leak(just in case)
		alloc_.for_each(safe_blfree_functor());
		alloc_.clear();
		activeList_ = freeList_ = NULL;
	}

	bool Initialize(uint32 reserve_particles) {
		Finalize();
		if (reserve_particles<PARTICLE_BATCH)
			reserve_particles = PARTICLE_BATCH;

		freeList_ = (Particle*) blMalloc(reserve_particles*sizeof(Particle));
		if (NULL==freeList_)
			return false; // out of memory!?

		alloc_.push_back(freeList_);
		uint32 ii=0;
		for (uint32 i=1; i<reserve_particles; ++i) {
			freeList_[ii++].next = freeList_ + i;
		}
		freeList_[ii].next = NULL;

		return true;
	}

	void Finalize() {
		alloc_.for_each(safe_blfree_functor());
		alloc_.clear();
		activeList_ = freeList_ = NULL;
	}

	bool UpdateParticles(float elapsedTime) {
        // update particles, move dead particles to freeList;
		Particle** ppParticle = &activeList_;
		while (*ppParticle) {
			Particle* particle = *ppParticle;
			if ((particle->age+=elapsedTime)>particle->life) {
				*ppParticle = particle->next;
			    RecycleParticle_(particle);
				continue;
			}

		    // update position only
			particle->pos += elapsedTime*particle->vel;

			ppParticle = &(particle->next);
		}
		return true;
	}

	bool UpdateParticles(float elapsedTime, math::Vector3 const& g) {
        // update particles, move dead particles to freeList;
		Particle** ppParticle = &activeList_;
		while (*ppParticle) {
			Particle* particle = *ppParticle;
			if ((particle->age+=elapsedTime)>particle->life) {
				*ppParticle = particle->next;
			    RecycleParticle_(particle);
				continue;
			}

		    // update position
			particle->pos += elapsedTime*particle->vel;

			// update velocity
			particle->vel += elapsedTime * g;

			ppParticle = &(particle->next);
		}
		return true;
	}

	bool UpdateParticles(ParticleUpdateCallback callback, float elapsedTime) {
		BL_ASSERT(callback);
        // update particles, move dead particles to freeList;
		Particle** ppParticle = &activeList_;
		while (*ppParticle) {
			Particle* particle = *ppParticle;
			if (callback(particle, elapsedTime)) {
				ppParticle = &(particle->next);
			}
			else {
				*ppParticle = particle->next;
			    RecycleParticle_(particle);
			}
		}
		return true;
	}

	// request a free particle
	Particle* GetFreeParticle() {
		Particle* p = NULL;
		if (freeList_) {
           p = freeList_;
		   freeList_ = freeList_->next;
		}
		else {
			freeList_ = (Particle*) blMalloc(PARTICLE_BATCH*sizeof(Particle));
			if (NULL==freeList_)
				return NULL; // out of memory!?

//			BL_LOG("+%d particles\n", PARTICLE_BATCH);
			alloc_.push_back(freeList_);
			int ii = 0;
			for (int i=1; i<PARTICLE_BATCH; ++i) {
				freeList_[ii++].next = freeList_ + i;
			}
			freeList_[ii].next = NULL;
			p = freeList_;
		    freeList_ = freeList_->next;
		}
		
		// just in case...
		p->age = p->life = 0.0f;
		
		// add to list...
		p->next = activeList_;
		activeList_ = p;
		return p;
	}

	// do not do anything stupid...
	Particle const* GetActiveList() const { return activeList_; }
};

// a helper to create particle texture
inline graphics::Texture2D* CreateParticleTexture(uint32 size=32, float radius=0.2f, float cutoff=0.8f) 
{
	system::MemoryBuffer mem_(true);
	uint8* pixels = (uint8*) mem_.GetWritePtr(size*size);
	if (NULL!=pixels) {
		graphics::Texture2D* tex = graphics::Texture2D::New(0);
		if (NULL!=tex) {
			int const last = size - 1;
			int const half = (size>>1);
			float const inv_r = 1.0f/half;
			float const radiusSq  = radius * radius;
			float const cutoffSq = cutoff * cutoff;
			float const inv_radiusSq = 1.0f/radiusSq;
			float const inv_cutoffSq = 1.0f/cutoffSq;
			float const norm_factor = 1.0f/(inv_radiusSq - inv_cutoffSq);
			float x, yy, r;
			std::memset(pixels, 0, size*size);
			for (int h=1; h<last; ++h) {
				yy = (h - half) * inv_r;
				yy = yy*yy;
				for (int w=1; w<last; ++w) {
					x = (w - half) * inv_r; 	
					r = x*x + yy;
					if (r<radiusSq) {
						pixels[h*size+w] = 255;
					}
					else if (r>cutoffSq) {
						pixels[h*size+w] = 0;
					}
					else {
						r = (1.0f/r - inv_cutoffSq)*norm_factor;
						BL_ASSERT(0.0f<=r && r<=1.0f)
						pixels[h*size+w] = (uint8) (255.0f*r);
					}
				}
			}
			if (tex->UpdateImage((uint16)size, (uint16)size, graphics::FORMAT_I8, pixels))
				return tex;

			// failed?
			tex->Release();
		}
	}
	return NULL;
}

}} // namespace mlabs::balai

#endif