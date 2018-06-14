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
 * @file	BLOpenGL.h
 * @desc    entrance header when you need OpenGL ES
 * @author	andre chen
 * @history	2012/01/09 rewrited for htc magiclabs balai project,
 *                     origin came from andre's personal nu engine.
 */
#ifndef BL_OPEN_GL_UTILS_H
#define BL_OPEN_GL_UTILS_H

#include "BLCore.h"

#ifndef BL_RENDERER_OPENGL
#error "BL_RENDERER_OPENGL not defined"
#endif

#ifdef BL_RENDERER_OPENGL_ES
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#ifdef BL_OS_APPLE_iOS
// unique place in iOS sdk?
#include <OpenGLES/ES2/gl2.h>
#include <OpenGLES/ES2/gl2ext.h>
#else
// where is opengl include? this may depend on each platform
// headers suggested by OpenGL ES spec 2.0.25
#include <GLES2/gl2.h>    // APIs for core OpenGL ES 2.0
#include <GLES2/gl2ext.h> // defines APIs for all registered OES, EXT, and vendor
                          // extensions compatible with OpenGL ES 2.0 
                          // (some extensions are only compatible with OpenGL ES 1.x)
                          // OES(approved by the Khronos OpenGL ES Working Group) and
                          // EXT(multiple vendors extension)
#endif
#else
#include <glew.h>
#endif

namespace mlabs { namespace balai { namespace graphics {

// check error report, return true if any GL errors persist.
bool GLReportErrors(char const* headline);

// kick out fullscreen quad drawing
void GLDrawFullScreenQuad();

// shader
GLuint CompileGLShader(GLenum type, char const* source, GLint const* length=NULL);
GLuint CreateGLProgram(GLuint vertex_shader, GLuint fragment_shader);
GLuint CreateGLProgram(char const* vs, char const* ps);
GLuint CreateGLProgram(char const* source, GLint vshLength, GLint pshLength);

// OpenGL error check(RAII)
class GLErrorCheck {
    char headline_[128]; // mind the capacity!!!

public:
    explicit GLErrorCheck(char const* s=NULL);
    bool Report() const;
    ~GLErrorCheck();
};

}}}

#endif