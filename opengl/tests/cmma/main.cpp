/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "matrix.h"
#include "cmma.h"
#include "fbo.h"
#include "platform.h"

using namespace MaliSDK;
using namespace android;

//CMAA stuff
const int   TEXTURE_WIDTH   = 256;  // NOTE: texture size cannot be larger than
const int   TEXTURE_HEIGHT  = 256;  // the rendering window size in non-FBO mode/* Animation variables. */
GLuint gTextureScreen;
ApplyFramebufferAttachmentCMAAINTELResourceManager *cmma = NULL;

#define FBO_WIDTH    256
#define FBO_HEIGHT    256

/* Shader variables. */
GLuint vertexShaderID = 0;
GLuint fragmentShaderID = 0;
GLuint programID = 0;
GLint iLocPosition = -1;
GLint iLocTextureMix = -1;
GLint iLocTexture = -1;
GLint iLocFillColor = -1;
GLint iLocTexCoord = -1;
GLint iLocProjection = -1;
GLint iLocModelview = -1;

/* Animation variables. */
static float angleX = 0;
static float angleY = 0;
static float angleZ = 0;
Matrix rotationX;
Matrix rotationY;
Matrix rotationZ;
Matrix translation;
Matrix modelView;
Matrix projection;
Matrix projectionFBO;

/* Framebuffer variables. */
GLuint iFBO = 0;
GLuint iFBOCMMA = 0;
GLuint rboId, rboColorId, rboDepthId;   // IDs of Renderbuffer objects
int msaaCount = 1;

/* Application textures. */
GLuint iFBOTex = 0;
GLuint iFBOTex_copy_from_fbo = 0;
GLuint rgba8_texture_main = 0;


int windowWidth = -1;
int windowHeight = -1;

static void printGLString(const char *name, GLenum s) {
    // fprintf(stderr, "printGLString %s, %d\n", name, s);
    const char *v = (const char *) glGetString(s);
    // int error = glGetError();
    // fprintf(stderr, "glGetError() = %d, result of glGetString = %x\n", error,
    //        (unsigned int) v);
    // if ((v < (const char*) 0) || (v > (const char*) 0x10000))
    //    fprintf(stderr, "GL %s = %s\n", name, v);
    // else
    //    fprintf(stderr, "GL %s = (null) 0x%08x\n", name, (unsigned int) v);
    fprintf(stderr, "GL %s = %s\n", name, v);
}

static void checkEglError(const char* op, EGLBoolean returnVal = EGL_TRUE) {
    if (returnVal != EGL_TRUE) {
        fprintf(stderr, "%s() returned %d\n", op, returnVal);
    }

    for (EGLint error = eglGetError(); error != EGL_SUCCESS; error
            = eglGetError()) {
        fprintf(stderr, "after %s() eglError %s (0x%x)\n", op, EGLUtils::strerror(error),
                error);
    }
}
static void checkGlError(const char* op) {
    for (GLint error = glGetError(); error; error
            = glGetError()) {
        fprintf(stderr, "after %s() glError (0x%x)\n", op, error);
    }
}

GLuint loadShader(GLenum shaderType, const char* pSource) {
    GLuint shader = glCreateShader(shaderType);
    if (shader) {
        glShaderSource(shader, 1, &pSource, NULL);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen) {
                char* buf = (char*) malloc(infoLen);
                if (buf) {
                    glGetShaderInfoLog(shader, infoLen, NULL, buf);
                    fprintf(stderr, "Could not compile shader %d:\n%s\n",
                            shaderType, buf);
                    free(buf);
                }
                glDeleteShader(shader);
                shader = 0;
            }
        }
    }
    return shader;
}

GLuint createProgram(const char* pVertexSource, const char* pFragmentSource) {
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader) {
        return 0;
    }

    GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!pixelShader) {
        return 0;
    }

    GLuint program = glCreateProgram();
    if (program) {
        glAttachShader(program, vertexShader);
        checkGlError("glAttachShader");
        glAttachShader(program, pixelShader);
        checkGlError("glAttachShader");
        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE) {
            GLint bufLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength) {
                char* buf = (char*) malloc(bufLength);
                if (buf) {
                    glGetProgramInfoLog(program, bufLength, NULL, buf);
                    fprintf(stderr, "Could not link program:\n%s\n", buf);
                    free(buf);
                }
            }
            glDeleteProgram(program);
            program = 0;
        }
    }
    return program;
}

GLuint gProgram;
GLuint gTextureProgram;
GLuint gvPositionHandle;
GLuint gvTexturePositionHandle;
GLuint gvTextureTexCoordsHandle;
GLuint gvTextureSamplerHandle;
GLuint gFbo;
GLuint gTexture;
GLuint gBufferTexture;

static const char gSimpleVS[] =
    "attribute vec4 a_v4Position;\n"
    "attribute vec4 a_v4FillColor;\n"
    "attribute vec2 a_v2TexCoord;\n"
    "uniform mat4 u_m4Projection;\n"
    "uniform mat4 u_m4Modelview;\n"
    "varying vec4 v_v4FillColor;\n"
    "varying vec2 v_v2TexCoord;\n"
    "\nvoid main(void) {\n"
    "    v_v4FillColor = a_v4FillColor;\n"
    "    v_v2TexCoord = a_v2TexCoord;\n"
    "    gl_Position = u_m4Projection * u_m4Modelview * a_v4Position;\n"
    "}\n\n";
static const char gSimpleFS[] =
    "precision mediump float;\n\n"
    "uniform sampler2D u_s2dTexture;\n"
    "uniform float u_fTex;\n"
    "varying vec4 v_v4FillColor;\n"
    "varying vec2 v_v2TexCoord;\n"
    "\nvoid main(void) {\n"
    "    vec4 v4Texel = texture2D(u_s2dTexture, v_v2TexCoord);\n"
    "    gl_FragColor = mix(v_v4FillColor, v4Texel, u_fTex);\n"
    "}\n\n";

bool setupGraphics(int w, int h) {

    windowWidth = w;
    windowHeight = h;

    programID = createProgram(gSimpleVS, gSimpleFS);
    if (!programID) {
		fprintf(stderr,"error creating shader programs\n");
        return false;
    }

    /* Initialize matrices. */
    projection = Matrix::matrixPerspective(45.0f, w/(float)h, 0.01f, 100.0f);
    projectionFBO = Matrix::matrixPerspective(45.0f, (TEXTURE_WIDTH / (float)TEXTURE_HEIGHT), 0.01f, 100.0f);
    /* Move cube 2 further away from camera. */
    translation = Matrix::createTranslation(0.0f, 0.0f, -2.0f);

    /* Initialize OpenGL ES. */
    GL_CHECK(glEnable(GL_CULL_FACE));
    GL_CHECK(glCullFace(GL_BACK));
    GL_CHECK(glEnable(GL_DEPTH_TEST));
    GL_CHECK(glEnable(GL_BLEND));
    /* Should do src * (src alpha) + dest * (1-src alpha). */
    GL_CHECK(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    /* Initialize FBO texture. */
    GL_CHECK(glGenTextures(1, &iFBOTex));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, iFBOTex));
    /* Set filtering. */
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
	
    GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, FBO_WIDTH, FBO_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL));

	//
    GL_CHECK(glGenTextures(1, &iFBOTex_copy_from_fbo));
    GL_CHECK(glBindTexture(GL_TEXTURE_2D, iFBOTex_copy_from_fbo));
	GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, FBO_WIDTH, FBO_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL));
    /* Set filtering. */
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));	
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
    

	/* Render to fbo CMMA*/
    GL_CHECK(glGenFramebuffers(1, &iFBOCMMA));
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, iFBOCMMA));

    // create a CMMA renderbuffer object to store color info
    GL_CHECK(glGenRenderbuffers(1, &rboColorId));
    GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, rboColorId));
    GL_CHECK(glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaaCount, GL_RGBA8_OES, TEXTURE_WIDTH, TEXTURE_HEIGHT));
    GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, 0));

    // create a CMMA renderbuffer object to store depth info
    // NOTE: A depth renderable image should be attached the FBO for depth test.
    // If we don't attach a depth renderable image to the FBO, then
    // the rendering output will be corrupted because of missing depth test.
    // If you also need stencil test for your rendering, then you must
    // attach additional image to the stencil attachement point, too.
    GL_CHECK(glGenRenderbuffers(1, &rboDepthId));
    GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, rboDepthId));
    GL_CHECK(glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaaCount, GL_DEPTH_COMPONENT16, TEXTURE_WIDTH, TEXTURE_HEIGHT));
    GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, 0));

	// attach cmaa RBOs to FBO attachment points
	GL_CHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rboColorId));
	GL_CHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepthId));	

    /* Check FBO is OK. */
    GLenum iResult = GL_CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER));
    if(iResult != GL_FRAMEBUFFER_COMPLETE)
    {
        fprintf(stderr,"Framebuffer incomplete at %s:%i\n", __FILE__, __LINE__);
        return false;
    }

    /* Initialize FBOs. */
    GL_CHECK(glGenFramebuffers(1, &iFBO));

    /* Render to framebuffer object. */
    /* Bind our framebuffer for rendering. */
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, iFBO));	

	GL_CHECK(glGenRenderbuffers(1, &rboId));
	GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, rboId));
	GL_CHECK(glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, TEXTURE_WIDTH, TEXTURE_HEIGHT));
	GL_CHECK(glBindRenderbuffer(GL_RENDERBUFFER, 0));

    /* Attach texture to the framebuffer. */
    GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, iFBOTex, 0));

    // attach a rbo to FBO depth attachement point
    GL_CHECK(glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboId));

    /* Check FBO is OK. */
    iResult = GL_CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER));
    if(iResult != GL_FRAMEBUFFER_COMPLETE)
    {
        fprintf(stderr,"Framebuffer incomplete at %s:%i\n", __FILE__, __LINE__);
        return false;
    }

    /* Unbind framebuffer. */
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, 0));

	/* need program to check status of shader attributes */
	GL_CHECK(glUseProgram(programID));
	
    /* Vertex positions. */
    iLocPosition = GL_CHECK(glGetAttribLocation(programID, "a_v4Position"));
    if(iLocPosition == -1)
    {
       fprintf(stderr,"Attribute not found at %s:%i\n", __FILE__, __LINE__);
        return false;
    }
    GL_CHECK(glEnableVertexAttribArray(iLocPosition));

    /* Texture mix. */
    iLocTextureMix = GL_CHECK(glGetUniformLocation(programID, "u_fTex"));
    if(iLocTextureMix == -1)
    {
        fprintf(stderr,"Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
    }
    else 
    {
        GL_CHECK(glUniform1f(iLocTextureMix, 0.0));
    }

    /* Texture. */
    iLocTexture = GL_CHECK(glGetUniformLocation(programID, "u_s2dTexture"));
    if(iLocTexture == -1)
    {
        fprintf(stderr,"Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
    }
    else
    {
        GL_CHECK(glUniform1i(iLocTexture, 0));
    }

    /* Vertex colors. */
    iLocFillColor = GL_CHECK(glGetAttribLocation(programID, "a_v4FillColor"));
    if(iLocFillColor == -1)
    {
        fprintf(stderr,"Warning: Attribute not found at %s:%i\n", __FILE__, __LINE__);
    }
    else 
    {
        GL_CHECK(glEnableVertexAttribArray(iLocFillColor));
    }

    /* Texture coords. */
    iLocTexCoord = GL_CHECK(glGetAttribLocation(programID, "a_v2TexCoord"));
    if(iLocTexCoord == -1)
    {
        fprintf(stderr,"Warning: Attribute not found at %s:%i\n", __FILE__, __LINE__);
    }
    else 
    {
        GL_CHECK(glEnableVertexAttribArray(iLocTexCoord));
    }

    /* Projection matrix. */
    iLocProjection = GL_CHECK(glGetUniformLocation(programID, "u_m4Projection"));
    if(iLocProjection == -1)
    {
        fprintf(stderr,"Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
    }
    else 
    {
        GL_CHECK(glUniformMatrix4fv(iLocProjection, 1, GL_FALSE, projection.getAsArray()));
    }

    /* Modelview matrix. */
    iLocModelview = GL_CHECK(glGetUniformLocation(programID, "u_m4Modelview"));
    if(iLocModelview == -1)
    {
        fprintf(stderr,"Warning: Uniform not found at %s:%i\n", __FILE__, __LINE__);
    }
    /* We pass this for each object, below. */

    glViewport(0, 0, w, h);
    checkGlError("glViewport");
    return true;
}

void renderFrame(GLint w, GLint h, int filter_method, int gl_copy) {
    /* Both main window surface and FBO use the same shader program. */
    GL_CHECK(glUseProgram(programID));

    /* Both drawing surfaces also share vertex data. */
    GL_CHECK(glEnableVertexAttribArray(iLocPosition));
    GL_CHECK(glVertexAttribPointer(iLocPosition, 3, GL_FLOAT, GL_FALSE, 0, cubeVertices));

    /* Including color data. */
    if(iLocFillColor != -1)
    {
        GL_CHECK(glEnableVertexAttribArray(iLocFillColor));
        GL_CHECK(glVertexAttribPointer(iLocFillColor, 4, GL_FLOAT, GL_FALSE, 0, cubeColors));
    }

    /* And texture coordinate data. */
    if(iLocTexCoord != -1)
    {
        GL_CHECK(glEnableVertexAttribArray(iLocTexCoord));
        GL_CHECK(glVertexAttribPointer(iLocTexCoord, 2, GL_FLOAT, GL_FALSE, 0, cubeTextureCoordinates));
    }

    /* Bind the FrameBuffer Object. */
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, iFBOCMMA));

    /* Set the viewport according to the FBO's texture. */
    GL_CHECK(glViewport(0, 0, FBO_WIDTH, FBO_HEIGHT));

    /* Clear screen on FBO. */
    GL_CHECK(glClearColor(0.5f, 0.5f, 0.5f, 1.0));
    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    /* Create rotation matrix specific to the FBO's cube. */
    rotationX = Matrix::createRotationX(-angleZ);
    rotationY = Matrix::createRotationY(-angleY);
    rotationZ = Matrix::createRotationZ(-angleX);

    /* Rotate about origin, then translate away from camera. */
    modelView = translation * rotationX;
    modelView = modelView * rotationY;
    modelView = modelView * rotationZ;

    /* Load FBO-specific projection and modelview matrices. */
    GL_CHECK(glUniformMatrix4fv(iLocModelview, 1, GL_FALSE, modelView.getAsArray()));
    GL_CHECK(glUniformMatrix4fv(iLocProjection, 1, GL_FALSE, projectionFBO.getAsArray()));

    /* The FBO cube doesn't get textured so zero the texture mix factor. */
    if(iLocTextureMix != -1)
    {
        GL_CHECK(glUniform1f(iLocTextureMix, 0.0));
    }

    /* Now draw the colored cube to the FrameBuffer Object. */
    GL_CHECK(glDrawElements(GL_TRIANGLE_STRIP, sizeof(cubeIndices) / sizeof(GLubyte), GL_UNSIGNED_BYTE, cubeIndices));
	
	//perform CMMA render pass on the already *bound* fbo
	if(filter_method)
		cmma->ApplyFramebufferAttachmentCMAAINTEL(/*iFBOCMMA*/&iFBOTex, &rgba8_texture_main, TEXTURE_WIDTH, TEXTURE_HEIGHT, GL_RGBA8_OES);    

	//fprintf(stderr, "main: rgba8_texture_main = %u\n",rgba8_texture_main);
    /* Both main window surface and FBO use the same shader program. */
    GL_CHECK(glUseProgram(programID));
	    /* Bind the FrameBuffer Object. */
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, iFBOCMMA));

	if(gl_copy){
		// copy rendered image from CMAA (multi-sample) to normal (single-sample) FBO
		// NOTE: The multi samples at a pixel in read buffer will be converted
		// to a single sample at the target pixel in draw buffer.
		//GL_CHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, iFBOCMMA));
		//GL_CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, iFBO));
		//GL_CHECK(glBlitFramebuffer(0, 0, TEXTURE_WIDTH, TEXTURE_HEIGHT, // src rect
		//				  0, 0, TEXTURE_WIDTH, TEXTURE_HEIGHT,	// dst rect
		//				  GL_COLOR_BUFFER_BIT, // buffer mask
		//				  GL_NEAREST/*GL_LINEAR*/));							// scale filter
		
		// trigger mipmaps generation explicitly
		// NOTE: If GL_GENERATE_MIPMAP is set to GL_TRUE, then glCopyTexSubImage2D()
		// triggers mipmap generation automatically. However, the texture attached
		// onto a FBO should generate mipmaps manually via glGenerateMipmap().
		//GL_CHECK(glBindTexture(GL_TEXTURE_2D, iFBOTex));
		//GL_CHECK(glGenerateMipmap(GL_TEXTURE_2D));
		//GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
		//GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, iFBO));
		// Copy content of FBO into a texture
		GL_CHECK(glActiveTexture(GL_TEXTURE0));
		GL_CHECK(glBindTexture(GL_TEXTURE_2D, rgba8_texture_main));
		GL_CHECK(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
	    GL_CHECK(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
	    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
	    GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
		GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER, iFBO));
        GL_CHECK(glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,rgba8_texture_main, 0));		
		GL_CHECK(glBindTexture(GL_TEXTURE_2D, iFBOTex_copy_from_fbo));
		//GL_CHECK(glBindTexture(GL_TEXTURE_2D, iFBOTex));
		GL_CHECK(glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, TEXTURE_WIDTH, TEXTURE_HEIGHT));

		//GL_CHECK(glBindTexture(GL_TEXTURE_2D, iFBOTex_copy_from_fbo));
		//GL_CHECK(glGenerateMipmap(GL_TEXTURE_2D));		
		//GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));
	}else{
	
		// copy rendered image from CMAA (multi-sample) to normal (single-sample) FBO
		// NOTE: The multi samples at a pixel in read buffer will be converted
		// to a single sample at the target pixel in draw buffer.
		GL_CHECK(glBindFramebuffer(GL_READ_FRAMEBUFFER, iFBOCMMA));
		GL_CHECK(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, iFBO));
		GL_CHECK(glBlitFramebuffer(0, 0, TEXTURE_WIDTH, TEXTURE_HEIGHT, // src rect
						  0, 0, TEXTURE_WIDTH, TEXTURE_HEIGHT,	// dst rect
						  GL_COLOR_BUFFER_BIT, // buffer mask
						  GL_NEAREST));			// scale filter
		
		// trigger mipmaps generation explicitly
		// NOTE: If GL_GENERATE_MIPMAP is set to GL_TRUE, then glCopyTexSubImage2D()
		// triggers mipmap generation automatically. However, the texture attached
		// onto a FBO should generate mipmaps manually via glGenerateMipmap().
		GL_CHECK(glBindTexture(GL_TEXTURE_2D, iFBOTex));
		GL_CHECK(glGenerateMipmap(GL_TEXTURE_2D));
		GL_CHECK(glBindTexture(GL_TEXTURE_2D, 0));

	}

    /* And unbind the FrameBuffer Object so subsequent drawing calls are to the EGL window surface. */
    GL_CHECK(glBindFramebuffer(GL_FRAMEBUFFER,0));

    /* Reset viewport to the EGL window surface's dimensions. */
    GL_CHECK(glViewport(0, 0, windowWidth, windowHeight));

    /* Clear the screen on the EGL surface. */
    GL_CHECK(glClearColor(0.0f, 0.0f, 1.0f, 1.0));
    GL_CHECK(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    /* Construct different rotation for main cube. */
    rotationX = Matrix::createRotationX(angleX);
    rotationY = Matrix::createRotationY(angleY);
    rotationZ = Matrix::createRotationZ(angleZ);

    /* Rotate about origin, then translate away from camera. */
    modelView = translation * rotationX;
    modelView = modelView * rotationY;
    modelView = modelView * rotationZ;

    /* Load EGL window-specific projection and modelview matrices. */
    GL_CHECK(glUniformMatrix4fv(iLocModelview, 1, GL_FALSE, modelView.getAsArray()));
    GL_CHECK(glUniformMatrix4fv(iLocProjection, 1, GL_FALSE, projection.getAsArray()));

    /* For the main cube, we use texturing so set the texture mix factor to 1. */
    if(iLocTextureMix != -1)
    {
        GL_CHECK(glUniform1f(iLocTextureMix, 1.0));
    }

    /* Ensure the correct texture is bound to texture unit 0. */
    GL_CHECK(glActiveTexture(GL_TEXTURE0));
	if(gl_copy){
	    GL_CHECK(glBindTexture(GL_TEXTURE_2D, iFBOTex_copy_from_fbo));
		//GL_CHECK(glBindTexture(GL_TEXTURE_2D, iFBOTex));
	}else{
		GL_CHECK(glBindTexture(GL_TEXTURE_2D, iFBOTex));
	}

    /* And draw the cube. */
    GL_CHECK(glDrawElements(GL_TRIANGLE_STRIP, sizeof(cubeIndices) / sizeof(GLubyte), GL_UNSIGNED_BYTE, cubeIndices));


    /* Update cube's rotation angles for animating. */
    angleX += .03;
    angleY += .02;
    angleZ += .01;

    if(angleX >= 360) angleX -= 360;
    if(angleY >= 360) angleY -= 360;
    if(angleZ >= 360) angleZ -= 360;

}

void printEGLConfiguration(EGLDisplay dpy, EGLConfig config) {

#define X(VAL) {VAL, #VAL}
    struct {EGLint attribute; const char* name;} names[] = {
    X(EGL_BUFFER_SIZE),
    X(EGL_ALPHA_SIZE),
    X(EGL_BLUE_SIZE),
    X(EGL_GREEN_SIZE),
    X(EGL_RED_SIZE),
    X(EGL_DEPTH_SIZE),
    X(EGL_STENCIL_SIZE),
    X(EGL_CONFIG_CAVEAT),
    X(EGL_CONFIG_ID),
    X(EGL_LEVEL),
    X(EGL_MAX_PBUFFER_HEIGHT),
    X(EGL_MAX_PBUFFER_PIXELS),
    X(EGL_MAX_PBUFFER_WIDTH),
    X(EGL_NATIVE_RENDERABLE),
    X(EGL_NATIVE_VISUAL_ID),
    X(EGL_NATIVE_VISUAL_TYPE),
    X(EGL_SAMPLES),
    X(EGL_SAMPLE_BUFFERS),
    X(EGL_SURFACE_TYPE),
    X(EGL_TRANSPARENT_TYPE),
    X(EGL_TRANSPARENT_RED_VALUE),
    X(EGL_TRANSPARENT_GREEN_VALUE),
    X(EGL_TRANSPARENT_BLUE_VALUE),
    X(EGL_BIND_TO_TEXTURE_RGB),
    X(EGL_BIND_TO_TEXTURE_RGBA),
    X(EGL_MIN_SWAP_INTERVAL),
    X(EGL_MAX_SWAP_INTERVAL),
    X(EGL_LUMINANCE_SIZE),
    X(EGL_ALPHA_MASK_SIZE),
    X(EGL_COLOR_BUFFER_TYPE),
    X(EGL_RENDERABLE_TYPE),
    X(EGL_CONFORMANT),
   };
#undef X

    for (size_t j = 0; j < sizeof(names) / sizeof(names[0]); j++) {
        EGLint value = -1;
        EGLint returnVal = eglGetConfigAttrib(dpy, config, names[j].attribute, &value);
        EGLint error = eglGetError();
        if (returnVal && error == EGL_SUCCESS) {
          //  printf(" %s: ", names[j].name);
         //   printf("%d (0x%x)", value, value);
        }
    }
  //  printf("\n");
}

int printEGLConfigurations(EGLDisplay dpy) {
    EGLint numConfig = 0;
    EGLint returnVal = eglGetConfigs(dpy, NULL, 0, &numConfig);
    checkEglError("eglGetConfigs", returnVal);
    if (!returnVal) {
        return false;
    }

    printf("Number of EGL configuration: %d\n", numConfig);

    EGLConfig* configs = (EGLConfig*) malloc(sizeof(EGLConfig) * numConfig);
    if (! configs) {
        printf("Could not allocate configs.\n");
        return false;
    }

    returnVal = eglGetConfigs(dpy, configs, numConfig, &numConfig);
    checkEglError("eglGetConfigs", returnVal);
    if (!returnVal) {
        free(configs);
        return false;
    }

    for(int i = 0; i < numConfig; i++) {
        //printf("Configuration %d\n", i);
      //  printEGLConfiguration(dpy, configs[i]);
    }

    free(configs);
    return true;
}

int main(int argc, char** argv) {
    EGLBoolean returnValue;
    EGLConfig myConfig = {0};

    EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    EGLint s_configAttribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 16,
            EGL_NONE };
    EGLint majorVersion;
    EGLint minorVersion;
    EGLContext context;
    EGLSurface surface;
    EGLint w, h;
	int c=0, filter_method = 0, gl_copy = 0;

	while ((c = getopt (argc, argv, "cb")) != -1)
	switch(c) {
		case 'c':
			//CMMA filter
			filter_method = 1;
			break;
		case 'b':
			//copy through glCopyTexSubImage
			gl_copy = 1;
			break;			
		default:
			filter_method = 0;
			gl_copy = 0;
	}

	fprintf(stderr,"filter: %d, gl_copy: %d\n",filter_method, gl_copy);

    EGLDisplay dpy;
   cmma = new ApplyFramebufferAttachmentCMAAINTELResourceManager();

    checkEglError("<init>");
    dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    checkEglError("eglGetDisplay");
    if (dpy == EGL_NO_DISPLAY) {
        printf("eglGetDisplay returned EGL_NO_DISPLAY.\n");
        return 0;
    }

    returnValue = eglInitialize(dpy, &majorVersion, &minorVersion);
    checkEglError("eglInitialize", returnValue);
    fprintf(stderr, "EGL version %d.%d\n", majorVersion, minorVersion);
    if (returnValue != EGL_TRUE) {
        printf("eglInitialize failed\n");
        return 0;
    }

    if (!printEGLConfigurations(dpy)) {
        printf("printEGLConfigurations failed\n");
        return 0;
    }

    //checkEglError("printEGLConfigurations");

    WindowSurface windowSurface;
    EGLNativeWindowType window = windowSurface.getSurface();
    EGLint numConfigs = -1, n = 0;
    eglChooseConfig(dpy, s_configAttribs, 0, 0, &numConfigs);
    if (numConfigs) {
        EGLConfig* const configs = new EGLConfig[numConfigs];
        eglChooseConfig(dpy, s_configAttribs, configs, numConfigs, &n);
        myConfig = configs[0];
        delete[] configs;
    }

    checkEglError("EGLUtils::selectConfigForNativeWindow");

   //printf("Chose this configuration:\n");
    //printEGLConfiguration(dpy, myConfig);

    surface = eglCreateWindowSurface(dpy, myConfig, window, NULL);
    checkEglError("eglCreateWindowSurface");
    if (surface == EGL_NO_SURFACE) {
        printf("gelCreateWindowSurface failed.\n");
        return 0;
    }

    context = eglCreateContext(dpy, myConfig, EGL_NO_CONTEXT, context_attribs);
    checkEglError("eglCreateContext");
    if (context == EGL_NO_CONTEXT) {
        printf("eglCreateContext failed\n");
        return 0;
    }
    returnValue = eglMakeCurrent(dpy, surface, surface, context);
    checkEglError("eglMakeCurrent", returnValue);
    if (returnValue != EGL_TRUE) {
        return 0;
    }
    eglQuerySurface(dpy, surface, EGL_WIDTH, &w);
    checkEglError("eglQuerySurface");
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &h);
    checkEglError("eglQuerySurface");
    GLint dim = w < h ? w : h;

    fprintf(stderr, "Window dimensions: %d x %d\n", w, h);

    //printGLString("Version", GL_VERSION);
    //printGLString("Vendor", GL_VENDOR);
    //printGLString("Renderer", GL_RENDERER);
    //printGLString("Extensions", GL_EXTENSIONS);
   
    if(!setupGraphics(w, h)) {
        fprintf(stderr, "Could not set up graphics.\n");
        return 0;
    }

	if(filter_method)
		cmma->Initialize();

    for (int i=0; i<300; i++) {
        renderFrame(w, h, filter_method, gl_copy);
        eglSwapBuffers(dpy, surface);
        checkEglError("eglSwapBuffers");
    }

   if(filter_method)
   	delete(cmma);
   
   glDeleteTextures(1, &gTexture);
   gTexture = 0;
   glDeleteTextures(1, &gTextureScreen);
   gTextureScreen = 0;

    return 0;
}
