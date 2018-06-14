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
 * @file	BLGLES2Mesh.h
 * @author	andre chen
 * @desc    mesh for OpenGL ES 2.0+
 * @history	2012/02/23 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_GLES2_MESH_H
#define BL_GLES2_MESH_H

#ifndef BL_ALLOW_INCLUDE_GLMESH_H
#error "Do NOT include 'BLGLES2Mesh.h' directly, #include 'BLMesh.h' instead"
#endif

#include "BLMesh.h"
#include "BLOpenGL.h"
#include "BLStreamFWD.h"

namespace mlabs { namespace balai { 

// forward declaration
namespace fileio { class AssetFile; }

namespace graphics {

class ShaderMaterial;

class GLES2Mesh : public IMesh
{
	// Rtti
	BL_DECLARE_RTTI;

	// batch
	struct DrawBatch {
		ShaderMaterial*	Mtl;
		uint8 const*	BonePalette;
		uint32			IndexOffset;
		uint32			VertexCount;
	};
//	BL_COMPILE_ASSERT(sizeof(DrawBatch)==4*sizeof(uint32), DrawBatch_size_not_OK);

	ShaderMaterial*	materials_;		// caution : polymorphism and array never mix!
	DrawBatch*		drawBatches_;   // draw batches, Save this, if only one batch?
	
    // allocated buffer
    uint8*          alloc_buffer_;

    //
    uint32			fvf_;				// vertex format code
	uint32			vertexStride_;
	uint32			totalDrawBatches_;	// # draw batches
    
    // vertex & index buffer objects
    GLuint          vbo_;
    GLuint          ibo_;
	
	// disable default/copy ctor and assignment operator
	BL_NO_DEFAULT_CTOR(GLES2Mesh);
	BL_NO_COPY_ALLOW(GLES2Mesh);

protected:
	~GLES2Mesh(); // use Release()

public:
	GLES2Mesh(uint32 name, uint32 group);

	// create mesh from resource file
	bool CreateFromStream(fileio::ifstream& stream, uint32 chunk_size,
                          fileio::MeshAttribute const& attr,
                          char const* stringPool, uint32 stringPoolSize);
	// slow/simple render
	bool Render(math::Matrix3 const*, float const*, DrawConfig const*, uint32 pass);

	uint32 BeginDraw(uint32 pass) const;
	bool DrawSubset(uint32 subset, math::Matrix3 const*, float const*, DrawConfig const*, uint32 pass, bool mtl) const;
};
    
}}} // namespace mlabs::balai::graphics

#endif // BL_GLES2_MESH_H