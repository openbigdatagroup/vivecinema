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
 * @file	BLAssetPackage.h
 * @author	andre chen
 * @history	2012/01/30 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_ASSET_PACKAGE_H
#define BL_ASSET_PACKAGE_H

#include "BLObject.h"
#include "BLString.h"
#include "BLStreamFWD.h"

namespace mlabs { namespace balai {

namespace graphics { class IMesh; }

namespace fileio {

#define MAKE4CC(a,b,c,d) ((a<<24)|(b<<16)|(c<<8)|d)

// file header
struct FileHeader {
	static uint32 const CURRENT_VERSION = 0xffffffff;

	static uint32 const PLATFORM_WIN32	= MAKE4CC('W', '3', '2', ' ');
	static uint32 const PLATFORM_WII	= MAKE4CC('R', 'V', 'L', ' ');
	static uint32 const PLATFORM_PS3	= MAKE4CC('P', 'S', '3', ' ');
	static uint32 const PLATFORM_PSP	= MAKE4CC('P', 'S', 'P', ' ');
	static uint32 const PLATFORM_X360	= MAKE4CC('X', '3', '6', '0');
    static uint32 const PLATFORM_iOS    = MAKE4CC('i', 'O', 'S', ' ');
    static uint32 const PLATFORM_Android = MAKE4CC('A', 'n', 'd', 'r');
	uint32	Platform;
	uint32	Version;
	uint32	Size;
	uint8	Date[4];		// [yy] [mm] [dd] [hr]
	char	Description[16];
};

// main chunk header
struct FileChunk {
	// string table
	static uint32 const STRING_TABLE_ID	   = MAKE4CC('s', 't', 'r', 't');

	// font
	static uint32 const FONT_CHUNK_ID	   = MAKE4CC('f', 'o', 'n', 't');
	  static uint32 const FONT_VERSION     = 1;

	// texture
	static uint32 const TEXTURE_CHUNK_ID   = MAKE4CC('t', 'e', 'x', 't');
	  static uint32 const TEXTURE_VERSION  = 0;

	// mesh
	static uint32 const MESH_CHUNK_ID	   = MAKE4CC('m', 'e', 's', 'h');
	  static uint32 const MESH_VERSION     = 0;

	// scene, world or venue
	static uint32 const SCENE_CHUNK_ID	   = MAKE4CC('s', 'c', 'n', 'e');
	  static uint32 const SCENE_VERSION	   = 0;
	  static uint32 const SPLINE_CHUNK_ID  = MAKE4CC('s', 'p', 'l', 'n');	// spline
	  static uint32 const CAMERA_CHUNK_ID  = MAKE4CC('c', 'a', 'm', 'r');	// camera
	  static uint32 const LIGHT_CHUNK_ID   = MAKE4CC('l', 'i', 't', 'e');	// light
	  static uint32 const DUMMY_CHUNK_ID   = MAKE4CC('d', 'u', 'm', 'i');	// dummy(name'd location)
	  static uint32 const BVHS_CHUNK_ID    = MAKE4CC('b', 'v', 'h', 's');	// bounding volume hierarchy structure
	  static uint32 const MTLS_CHUNK_ID    = MAKE4CC('m', 't', 'l', 's');	// material chunk

	// model(cross platform)
	static uint32 const MODEL_CHUNK_ID	   = MAKE4CC('m', 'o', 'd', 'l');
	  static uint32 const MODEL_VERSION	   = 0;
      static uint32 const SKELETON_CHUNK_ID= MAKE4CC('s', 'k', 'e', 'l');

	// animation(cross platform)
	static uint32 const ANIMATION_CHUNK_ID = MAKE4CC('a', 'n', 'i', 'm');
	  static uint32 const ANIM_VERSION	   = 0;

	// BGM
	static uint32 const BGM_CHUNK_ID	   = MAKE4CC('b', 'g', 'm', ' ');
	  static uint32 const BGM_VERSION	   = 0;

	// more to come...

	uint32	ID;			// chunk ID(see above IDs)
	uint32	Version;	// version & target platform info
	uint32	Elements;	// Number of "thing" in file
	uint32	Size;		// Total data size in this chunk(except chunk header itself)
	char	Description[16];
	uint32 ChunkSize() const { return BL_ALIGN_UP(Size, 32); }
};
#undef MAKE4CC

// inserter template
template<typename istream, typename header>
istream& operator>>(istream& s, header& hdr) {
	BL_COMPILE_ASSERT(0==(sizeof(header)%32), header_size_error);
	s.Read(&hdr, sizeof(header));
	return s;
}

// user chunk...
class AssetFile;
typedef bool (*ProcessUserChunkCB)(AssetFile*, ifstream& stream, FileChunk const& chunk);

// base resource file class use "builder", "abstract factory" pattern
class AssetFile : public IShared
{
	// RTTI
	BL_DECLARE_RTTI;

	String			   filename_;
	ProcessUserChunkCB process_user_chunk_;
	char*			   stringTable_;
	uint32			   stringTableSize_;

protected:
	uint32 const seriesID_;

private:
	// builder
	virtual bool BuildHeader_(FileHeader const& hdr) const = 0;
	virtual void FinishBuild_() = 0;

	// textures(platform dependent)
	virtual bool ProcessTextureChunk_(ifstream& stream, FileChunk const& chunk) = 0;

	// mesh(platform dependent)
	virtual graphics::IMesh* CreateMeshFromStream_(ifstream& stream, char const* stringPool, uint32 stringPoolSize) = 0;

	// model(platform independent, contains platform dependent mesh chunks)
	bool ProcessModelChunk_(ifstream& stream, FileChunk const& chunk);

    // animation(platform independent)
	bool ProcessAnimationChunk_(ifstream& stream, FileChunk const& chunk);

	// world(platform independent, contains platform dependent world meshes)
	bool ProcessWorldSectorChunk_(ifstream& stream, FileChunk const& chunk);

	// disable copy ctor and assignment operator
	BL_NO_DEFAULT_CTOR(AssetFile);
	BL_NO_COPY_ALLOW(AssetFile);

protected:
	AssetFile(ProcessUserChunkCB userChunkCB, uint32 id);
	virtual ~AssetFile(); // use Release();

	// get name
	char const* LookupName_(uint32 name) const {
		if (name>=stringTableSize_ || NULL==stringTable_)
			return NULL;
		return (stringTable_ + name);
	}

public:
	// return series id - 0 for fail
	uint32 Build(char const* filename);
	char const* FileName() const { return filename_.c_str(); }
};

// load resource - all loaded resources share the same series id
// return 0 to indicate errors
// if success, it
//		1. return internal id series, if seriesId=0. or
//		2. return seriesId is not zero
//
uint32 blLoadAssets(char const* filename, ProcessUserChunkCB=NULL, uint32 seriesId=0);


}}} // namespace mlabs::balai::fileio

#endif // BL_ASSET_PACKAGE_H