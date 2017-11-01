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
 * @file	BLGLTexture.h
 * @author	andre chen
 * @history	2012/01/16 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_OPENGL_TEXTURE_H
#define BL_OPENGL_TEXTURE_H

#ifndef BL_ALLOW_INCLUDE_GLTEXTURE_H
#error "Do NOT include 'BLGLTexture.h' directly, #include 'BLTexture.h' instead"
#endif


#include "BLTexture.h"
#include "BLStreamFWD.h"
#include "BLOpenGL.h"

//
// OpenGL ES 2.0 Preprocessor tokens : GL_ES_VERSION_2_0
//

namespace mlabs { namespace balai { namespace graphics {

class GLTexture : public ITexture
{
    // Rtti
    BL_DECLARE_RTTI;

    GLuint  glTexture_;
    uint16  width_;
    uint16  height_;
    uint16  depth_;
    uint8   LODs_;
    uint8   alphaBits_;

    bool SetFilterMode_(TEXTURE_FILTER&);
    bool SetAddressMode_(TEXTURE_ADDRESS&, TEXTURE_ADDRESS&, TEXTURE_ADDRESS&);

    // disable default/copy ctor and assignment operator
    BL_NO_DEFAULT_CTOR(GLTexture);
    BL_NO_COPY_ALLOW(GLTexture);

protected:
    ~GLTexture(); // use Release()

public:
    GLTexture(uint32 name, uint32 group);
    uint16  Width() const    { return (0==glTexture_) ? 0:width_; }
    uint16  Height() const   { return (0==glTexture_) ? 0:height_; }
    uint16  Depth() const    { return (0==glTexture_) ? 0:depth_; }
    uint8   LODs() const     { return (0==glTexture_) ? 0:LODs_; }
    uint8   AlphaBits() const { return (0==glTexture_) ? 0:alphaBits_; }

    // create texture from asset file
    bool CreateFromStream(fileio::ifstream&, fileio::TextureAttribute const&);
};

}}} // namespace mlabs::balai::graphics

#endif //!BL_OPENGL_TEXTURE_H