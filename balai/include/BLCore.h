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
 * @file	BLCore.h
 * @desc    platforms, toolchain, build configurations settings and defines
 * @author	andre chen
 * @history	2005/09/06 original NuPlatforms.h created
 *          2011/12/27 rewrited for htc magiclabs balai project
 * @history 2012/08/09 add BL_HTC_NEW_HOTFIX for compiling on Android
 *					   mm build because htc has customized <new>
 *
 */

//
// Platform-specific information goes in this header file. Defines to
// control which platform is included are:
//
// BL_OS_WIN32 : Microsoft Windows 2000/XP/7.
// BL_OS_MANGO : Microsoft Windows 7/2 phone
// BL_OS_ANDROID : Google Android
// BL_OS_APPLE_iOS : apple iOS platform(TBD)

#ifndef BL_CORE_H
#define BL_CORE_H

#include <cassert>
#include <cstdio>
#include <cstdlib>

// delete/release
#ifndef BL_SAFE_DELETE
#define BL_SAFE_DELETE(p)		 { if(p) { delete (p); (p)=NULL; } }
#endif

#ifndef BL_SAFE_DELETE_ARRAY
#define BL_SAFE_DELETE_ARRAY(p) { if(p) { delete[] (p); (p)=NULL; } }
#endif

#ifndef BL_SAFE_RELEASE
#define BL_SAFE_RELEASE(p)		 { if(p) { (p)->Release(); (p)=NULL; } }
#endif

//
// round up/round down
//
// Note : these 3 macros doesn't work for 64bit size, unless you set (size_t) align!!!
//
#define BL_ALIGN_UP(size, align)   (((size)+(align)-1)&~((align)-1))
#define BL_ALIGN_DOWN(size, align) ((size)&~((align)-1))
#define BL_PAD_SIZE(size, align)   ((((size)+(align)-1)&~((align)-1)) - size)

// power of 2
#define BL_IS_POWER_OF_2(s)        ((0<s)&&(0==(s&(s-1))))

// min/max
#define BL_MAX(a, b)               (((a)>(b)) ? (a):(b))
#define BL_MIN(a, b)               (((a)<(b)) ? (a):(b))

// make 4CC
#define BL_MAKE_4CC(a,b,c,d) uint32((a<<24)|(b<<16)|(c<<8)|d)

// Disable default constructor
#define BL_NO_DEFAULT_CTOR(CLASS) \
private: \
    CLASS()

// Disable copy constructor and assignment operator
#define BL_NO_COPY_ALLOW(CLASS) \
private: \
    CLASS(CLASS const &); \
    CLASS& operator=(CLASS const&)

// Disable dynamic allocation on the heap
#define BL_NO_HEAP_ALLOC() \
private: \
	static void *operator new(std::size_t); \
	static void *operator new[](std::size_t)

//----------------------------------------------------------------------------
// Microsoft Windows 32bits/64bits platform
//----------------------------------------------------------------------------
#if defined(BL_OS_WIN32) || defined(BL_OS_WIN64) || defined(WIN32) || defined(WIN64) || defined(_WIN64)

#if defined(WIN64) || defined(_WIN64)
#ifndef BL_OS_WIN64
#define BL_OS_WIN64
#endif

#ifdef BL_OS_WIN32
#undef BL_OS_WIN32
#endif

#else // win32

#ifndef BL_OS_WIN32
#define BL_OS_WIN32
#endif

#ifdef BL_OS_WIN64
#undef BL_OS_WIN64
#endif

#endif

    #include <windows.h>

#ifndef BL_NO_SIMD
    #define BL_SSE2
    #include <emmintrin.h>
#endif

    #define BL_USE_EXCEPTIONS

    // exception specfication(vc compiler complains about exception spec other than throw())
    #define BL_EXCEPTION_SPEC_BADALLOC
    #define BL_EXCEPTION_SPEC_NOTHROW  throw()

    // core debug halt
    #define BL_CORE_HALT() __debugbreak()

#ifndef BL_RENDERER_D3D
    #define BL_RENDERER_OPENGL
    #ifdef BL_ADRENO_SDK_EGL
        #define BL_RENDERER_OPENGL_ES
    #endif
#endif

    // log
    #include <cstdarg>
	inline void BL_LOG(char const* format, ...) {
		char buffer[2048]; // 2K buffer

		// parse the given string
		std::va_list va;
		
		va_start(va, format);

		// disable warning C4996, darn _CRT_SECURE_NO_WARNINGS things!
#pragma warning(push)
#pragma warning(disable:4996)
		vsnprintf(buffer, 2047, format, va);	// not C++ standard, but compile PS3(GNU) and Wii(CodeWarrior)
	//	std::vsprintf(buffer, format, va);
#pragma warning(pop)
		va_end(va);

		buffer[2047] = '\0';
		OutputDebugStringA(buffer);
	}
	#define BL_ERR BL_LOG

	// primitive types
    #ifndef BL_DEFINE_PRIMITIVE_TYPE
    #define BL_DEFINE_PRIMITIVE_TYPE
	typedef signed char			int8, sint8;
	typedef unsigned char		uint8;
	typedef signed short		int16, sint16;
	typedef unsigned short		uint16;
	typedef signed int			int32, sint32;
	typedef unsigned int		uint32;
	typedef signed __int64		int64, sint64;
	typedef unsigned __int64	uint64;
    #endif

	// mutex
	namespace mlabs { namespace balai { namespace system {
	class Mutex {
		HANDLE mutex_;

	public:
		Mutex():mutex_(::CreateMutex(NULL, FALSE, NULL)) {
			if (NULL==mutex_) {
                BL_CORE_HALT();
            }
		}
		~Mutex() { 
			if (NULL!=mutex_) {
				::CloseHandle(mutex_);
				mutex_ = NULL;
			}
		}
        bool IsValid() const { return (NULL!=mutex_); }
        int Lock(bool blocking=true) {
			if (mutex_) {
                switch (::WaitForSingleObject(mutex_, blocking ? INFINITE:0))
                {
                // The thread got mutex ownership.
                case WAIT_OBJECT_0:
                    return 1;

                // Cannot get mutex ownership due to time-out
                case WAIT_TIMEOUT:
                    return 0;

                // Got ownership of the abandoned mutex object!?
                case WAIT_ABANDONED:
                    __debugbreak();
                    break;

                default: // WAIT_FAILED - no good!
                    __debugbreak();
                    break;
                }
            }
            return -1;
		}
		void Unlock() {
			if (mutex_) {
				::ReleaseMutex(mutex_);
			}
		}
	};
	}}} // namespace mlabs::balai::system

//----------------------------------------------------------------------------
// Android : little-endian ARM GNU/Linux ABI
//----------------------------------------------------------------------------
#elif defined(BL_OS_ANDROID) || defined(__ANDROID__)
#ifndef BL_OS_ANDROID
    #define BL_OS_ANDROID
#endif

#ifndef BL_NO_SIMD
#ifdef ARM_NEON_ENABLE
    #define BL_NEON
    #include <arm_neon.h>
#endif
#endif

    #include <pthread.h>
    #include <android/log.h>

    // if not using exception(-fno-exceptions), do modify
#ifdef BL_HTC_NEW_HOTFIX
	// -fno-exceptions
	#ifdef BL_USE_EXCEPTIONS
	#undef BL_USE_EXCEPTIONS
	#endif
	#define BL_EXCEPTION_SPEC_BADALLOC /* throw(std::bad_alloc) */
    #define BL_EXCEPTION_SPEC_NOTHROW  /* throw() */
#else
	// -fexceptions
    #define BL_USE_EXCEPTIONS
    #define BL_EXCEPTION_SPEC_BADALLOC throw(std::bad_alloc)
    #define BL_EXCEPTION_SPEC_NOTHROW  throw()
#endif
    // core debug halt
    #define BL_CORE_HALT() { std::exit(1); }

    #define BL_RENDERER_OPENGL
    #define BL_RENDERER_OPENGL_ES

    // log
    #define BL_LOG(...)  __android_log_print(ANDROID_LOG_DEBUG,"htc.mlabs.balai",__VA_ARGS__)
	#define BL_ERR(...)  __android_log_print(ANDROID_LOG_ERROR,"htc.mlabs.balai",__VA_ARGS__)

    // primitive types
    #ifndef BL_DEFINE_PRIMITIVE_TYPE
    #define BL_DEFINE_PRIMITIVE_TYPE
    typedef signed char			int8, sint8;
    typedef unsigned char		uint8;
    typedef signed short		int16, sint16;
    typedef unsigned short		uint16;
    typedef signed int			int32, sint32;
    typedef unsigned int		uint32;
    typedef signed long long	int64, sint64;
    typedef unsigned long long	uint64;
    #endif

//----------------------------------------------------------------------------
// pending PLATFORM here...
//----------------------------------------------------------------------------
// #elif defined(BL_OS_APPLE_iOS) ???
	

#else // platform not defined
#error "unknown platform"
#endif

// s5.10
class f16 { 
	uint16 half_;

public:
	f16():half_(0) {}
	f16(float x) {
		// use union : avoid dereference type-punned pointer that breaks strict-aliasing rules.
		union {
			float f32;
			uint32 u32;
		} f2u;

		f2u.f32 = x;
		half_ = (uint16) (f2u.u32&0x80000000>>16);  // sign
		int e = ((f2u.u32&0x7F800000) >> 23) - 112; // exponent f32 bias : -128, f16 bias:-16
		int m = (f2u.u32&0x007FFFFF);               // mantissa
		if (e <= 0) {
			// Denormed half
			m = ((m | 0x00800000) >> (1 - e)) + 0x1000;
			half_ |= (m >> 13);
		} 
		else if (e == 255 - 112) {
			half_ |= 0x7C00;
			if (m != 0) {
				// NAN
				m >>= 13;
				half_ |= m | (m == 0);
			}
		} 
		else {
			m += 0x1000;
			if (m & 0x00800000) {
				// Mantissa overflow
				m = 0;
				e++;
			}
			if (e >= 31)
				half_ |= 0x7C00;	// Exponent overflow
			else
				half_ |= (e << 10) | (m >> 13);
		}
	}
	operator float() const {
		union {
			float f32;
			uint32 u32;
		} u2f;

		u2f.u32 = (half_ & 0x8000) << 16;
		uint32 e = (half_ >> 10) & 0x1F;
		uint32 m = half_ & 0x03FF;
		if (e == 0) {
			// +/- 0
			if (m == 0) return u2f.f32;

			// Denorm
			while ((m & 0x0400) == 0) {
				m += m;
				e--;
			}
			++e;
			m &= ~0x0400;
		}
		else if (e == 31) {
			// INF / NAN
			u2f.u32 |= 0x7F800000 | (m << 13);
			return u2f.f32;
		}

		u2f.u32 |= ((e + 112) << 23) | (m << 13);
		return u2f.f32;
	}
};

// count bits
inline int BL_COUNT_BITS(uint32 x) {
    x -= (x >> 1) & 0x55555555;
    x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
    x = (x + (x >> 4)) & 0x0F0F0F0F;
    x += x >> 8;
    return (x + (x >> 16)) & 0x3F;
}
inline int BL_COUNT_BITS(uint64 x) {
    return BL_COUNT_BITS((uint32)(x&0xffffffff)) + BL_COUNT_BITS((uint32)(x>>32));
}

// use pthread?
#ifdef PTHREAD_MUTEX_INITIALIZER
#include <errno.h>

namespace mlabs { namespace balai { namespace system {
    class Mutex 
    {
        pthread_mutex_t mutex_;
        volatile int    state_;

    public:
        Mutex():mutex_(),state_(0) {
            if (0!=pthread_mutex_init(&mutex_, NULL)) {
                BL_CORE_HALT();
                state_ = -1;
            }
        }
        ~Mutex() {
            // It shall be safe to destroy an initialized mutex that is unlocked.
            // Attempting to destroy a locked mutex results in undefined behavior.
            pthread_mutex_destroy(&mutex_);
            state_ = -1;
        }
        bool IsValid() const { return (0<=state_); }
        int Lock(bool blocking=true) {
            if (0<=state_) {
                if (blocking) {
                    // if current state_ is locked(1), we will wait...
                    if (0==pthread_mutex_lock(&mutex_)) {
                        state_ = 1; // state_ is protected
                        return 1;
                    }
                }
                else {
                    int const res = pthread_mutex_trylock(&mutex_);
                    if (0==res) {
                        state_ = 1; // state_ is protected
                        return 1;
                    }
                    if (EBUSY==res) {
                        return 0;
                    }
                }
                // fail
                BL_CORE_HALT();
            }
            return -1; // error
        }
        void Unlock() {
            if (0==pthread_mutex_unlock(&mutex_)) {
                // __sync_fetch_and_and(&state_, 0);
				state_ = 0;
            }
        }
    };
}}} // namespace mantis::system

#endif

///////////////////////////////////////////////////////////////////////////////
// RAII for Mutex lock
// If you get any compile errors here, implement mutex class for your platform
///////////////////////////////////////////////////////////////////////////////
namespace mlabs { namespace balai { namespace system {
	class MUTEX_LOCK {
		Mutex& mutex_;
		
		// disable default/copy ctor and assignment operator
		BL_NO_DEFAULT_CTOR(MUTEX_LOCK);
		BL_NO_COPY_ALLOW(MUTEX_LOCK);
        BL_NO_HEAP_ALLOC();

	public:	
		MUTEX_LOCK(Mutex& mutex):mutex_(mutex) { mutex_.Lock(); }
		~MUTEX_LOCK() { mutex_.Unlock(); }
	};
}}}
#define BL_MUTEX_LOCK(mutex)  mlabs::balai::system::MUTEX_LOCK mutex_lock_guard__(mutex)

///////////////////////////////////////////////////////////////////////////////
// Debug/Release build
///////////////////////////////////////////////////////////////////////////////
#if defined(DEBUG)||defined(_DEBUG)||defined(__DEBUG)||defined(NRELEASE)||defined(DEBUG_BUILD)
	#define BL_DEBUG_BUILD
    #ifdef BL_RELEASE_BUILD
	#undef BL_RELEASE_BUILD
    #endif
#else
	#define BL_RELEASE_BUILD
    #ifdef BL_DEBUG_BUILD
	#undef BL_DEBUG_BUILD
    #endif
#endif

///////////////////////////////////////////////////////////////////////////////
// EXCEPTION MACROS:
///////////////////////////////////////////////////////////////////////////////
#ifdef BL_USE_EXCEPTIONS
    #undef BL_USE_EXCEPTIONS
	
    #define BL_TRY_BEGIN	try {
	#define BL_CATCH(x)		} catch (x) {
    
	#define BL_CATCH_ALL	} catch (...) {
	#define BL_CATCH_END	}

    #define BL_THROW_BADALLOC throw (std::bad_alloc())
    #define BL_RETHROW        throw

//
//	#define BL_RAISE(x)		throw (x)
//	#define BL_RERAISE		throw
    
//	#define BL_THROW0()		throw ()
//	#define BL_THROW1(x)	throw (x)
//	#define BL_THROW(x, y)	throw x(y)
//
#else
	#define BL_TRY_BEGIN	{{
	#define BL_CATCH(x)		} if (0) {
	#define BL_CATCH_ALL	} if (0) {
	#define BL_CATCH_END	}}
    #define BL_THROW_BADALLOC BL_CORE_HALT()
    #define BL_RETHROW        BL_CORE_HALT()

//
//	#ifdef __GNUC__
//		#if __GNUC__ < 3 && !defined(__APPLE__) && !defined(__MINGW32__)
//			#define BL_RAISE(x) _XSTD _Throw(x)
//		#else /* __GNUC__ < 3 && !defined(__APPLE__) && !defined(__MINGW32__) */
//			#define BL_RAISE(x)	::std:: _Throw(x)
//		#endif /* __GNUC__ < 3 && !defined(__APPLE__) && !defined(__MINGW32__) */
//	#else
//		#define BL_RAISE(x) BL_CORE_HALT()
//	#endif
//
//	#define ML_RERAISE BL_CORE_HALT()
//
//
//	#define ML_THROW0()
//	#define ML_THROW1(x)
//	#define ML_THROW(x, y)	x(y)._Raise()
//
 #endif


///////////////////////////////////////////////////////////////////////////////
// ASSERT MACROS:
///////////////////////////////////////////////////////////////////////////////
#ifdef BL_DEBUG_BUILD
	namespace mlabs { namespace balai { namespace debug {
		// assert handler - return true to halt system
		typedef bool (*AssertCallback)(char const*, char const*, char const*, int);
		
		// set user's assert handler
		void set_user_assert_handler(AssertCallback handler);

		// main assert handler
		bool assert_handler(char const*, char const*, char const*, int);
	}}}

    #define BL_ASSERT(exp) { \
        if (!(exp) && mlabs::balai::debug::assert_handler(#exp,NULL,__FILE__,__LINE__)) { \
			BL_CORE_HALT(); \
	    } \
    }

	#define BL_ASSERT2(exp,msg) { \
		if (!(exp) && mlabs::balai::debug::assert_handler(#exp,msg,__FILE__,__LINE__)) { \
			BL_CORE_HALT(); \
	    } \
	}

	#define BL_HALT(msg) { \
        mlabs::balai::debug::assert_handler(NULL,msg,__FILE__,__LINE__); \
		BL_CORE_HALT(); \
	}

#else
	#define BL_ASSERT(exp)      {}
	#define BL_ASSERT2(exp,msg) {}
	#define BL_HALT(msg)        {}
#endif

///////////////////////////////////////////////////////////////////////////////
// Compile-time assertion:
// expr is a compile-time integral or pointer expression
// msg is a C++ identifier that does not need to be defined
// ex.
//	BL_COMPILE_ASSERT(4==sizeof(int), INT_SIZE_IS_NOT_4);
// You'll have compile errors(VC8) like...
// error C2087: 'ERROR__INT_SIZE_IS_NOT_4' : missing subscript
///////////////////////////////////////////////////////////////////////////////
#ifndef BL_COMPILE_ASSERT
#ifdef BL_FINAL_RELEASE
	#define BL_COMPILE_ASSERT(expr, msg)
#else
	#if defined(BL_OS_WIN32)
		#define BL_COMPILE_ASSERT(expr, msg) typedef char ERROR__##msg[1][(expr)]
	#else
		#define BL_COMPILE_ASSERT(expr, msg) typedef char ERROR__##msg[(expr)?1:-1]
	#endif
#endif
#endif

///////////////////////////////////////////////////////////////////////////////
// is primitive type? type trait
// ref http://www.boost.org/doc/libs/1_37_0/libs/type_traits/index.htm
///////////////////////////////////////////////////////////////////////////////
template <typename T>
struct is_primitive_type
{
	static bool const value = false;
}; 
#define D_(type) \
	template <> \
	struct is_primitive_type<type> \
	{ \
		static bool const value = true; \
	} 
D_(char); D_(signed char); D_(unsigned char); 
D_(short); D_(unsigned short); 
D_(int); D_(unsigned int); 
D_(long); D_(unsigned long);
D_(int64); D_(uint64);
D_(float); D_(double);
// typedefs type is exactly the same 
#undef D_ 

///////////////////////////////////////////////////////////////////////////////
// bad values
///////////////////////////////////////////////////////////////////////////////
static uint8  const BL_BAD_UINT8_VALUE  = 0xff;
static uint16 const BL_BAD_UINT16_VALUE = 0xffff;
static uint32 const BL_BAD_UINT32_VALUE = 0xffffffff;
static uint64 const BL_BAD_UINT64_VALUE = 0xffffffffffffffffULL;

///////////////////////////////////////////////////////////////////////////////
// alignment
///////////////////////////////////////////////////////////////////////////////
/*
// alignment code
BL_ALIGN_BEGIN(64) class MyClass {
	//...
} BL_ALIGN_END(64);


// .NET(PreAlign)
_MSC_VER	// preprocessor .NET2003(VC7) = 1300, .NET2005(VC8) = 1400
#define BL_ALIGN_BEGIN(_align)  __declspec(align(_align))
#define BL_ALIGN_END(_align)

// SN Systems ProDG/GNU(PostAlign)
__GNUC__	// preprocessor
#define BL_ALIGN_BEGIN(_align)  
#define BL_ALIGN_END(_align)	__attribute__(__aligned__(_align))

// Metrowerks CodeWarrior(PostAlign)
__MWERKS__	// preprocessor
#define BL_ALIGN_BEGIN(_align)  
#define BL_ALIGN_END(_align)	__attribute__(aligned(_align))
*/

///////////////////////////////////////////////////////////////////////////////
// time function - platform dependent
///////////////////////////////////////////////////////////////////////////////
namespace mlabs { namespace balai { namespace system {
double GetTime(bool reset=false);
}}}

#endif // BL_CORE_H
