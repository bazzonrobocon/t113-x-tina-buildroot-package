/*
 * This confidential and proprietary software should be used
 * under the licensing agreement from Allwinner Technology.

 * Copyright (C) 2020-2030 Allwinner Technology Limited
 * All rights reserved.

 * Author:zhengwanyu <zhengwanyu@allwinnertech.com>

 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from Allwinner Technology Limited.
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <linux/fb.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <EGL/eglext.h>
#include <GLES2/gl2ext.h>
#include <EGL/eglplatform.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "drm_fourcc.h"
#include "REShader.h"
#include "renderengine.h"

using namespace std;

#define DEBUG_ENABLE 0

static unsigned char render_debug;

#define ARRAY_SIZE(array) (sizeof(array)/sizeof(array[0]))

#if DEBUG_ENABLE
#define RE_INF(...) \
do { \
	if (render_debug) {\
		printf("[RE]"); \
		printf(__VA_ARGS__); \
	} \
} while(0);
#else
#define RE_INF(...)
#endif

#define RE_ERR(...) \
	do {printf("[RE ERROR]"); printf(__VA_ARGS__);} while(0);

struct FormatName {
	unsigned int drm_format;
	const char name[10];
};

struct FormatName DrmFormat[] = {
	{DRM_FORMAT_ARGB8888, "argb8888"}, //fbdev、LCD default format
	{DRM_FORMAT_RGBA8888, "rgba8888"},
	{DRM_FORMAT_BGRA8888, "bgrb8888"},
	{DRM_FORMAT_RGB888,   "rgb888"},

	{DRM_FORMAT_YUV420,   "yv12"	},
	{DRM_FORMAT_NV21,     "nv21"    },
	{DRM_FORMAT_NV12,     "nv12"    },

	{DRM_FORMAT_NV61,     "nv61"	},
	{DRM_FORMAT_NV61,     "nv16"    },
};


EGLNativeWindowType native_window;

/*static const char *vertex_shader_offscreen_source =
	"attribute vec4 aPosition;                               \n"
	"attribute vec2 aTexCoord;                               \n"
	"uniform mat4 projection;				 \n"

	"varying vec2 vTexCoord;                                 \n"

	"void main()                                             \n"
	"{                                                       \n"
	"	vTexCoord = aTexCoord;                           \n"
	"	gl_Position = projection * vec4(aPosition.x, -aPosition.y, aPosition.zw);\n"
	"}                                                       \n";

static const char *vertex_shader_onscreen_source =
	"attribute vec4 aPosition;                               \n"
	"attribute vec2 aTexCoord;                               \n"
	"uniform mat4 projection;				 \n"

	"varying vec2 vTexCoord;                                 \n"

	"void main()                                             \n"
	"{                                                       \n"
	"	vTexCoord = aTexCoord;                           \n"
	"	gl_Position = projection * aPosition;	 	 \n"
	"}                                                       \n";*/

// vertex and fragment shader resource
static const char gVertexShaderOnScreen[] =
    "#version 300 es                                         \n"
    "in vec4 vPosition;                                      \n"
    "in vec2 vTexCoords;                                     \n"
    "uniform mat4 projection;                                \n"

    "out vec2 yuvTexCoords;                                  \n"
    "void main() {                                           \n"
    "  yuvTexCoords = vTexCoords;                            \n"
    "  gl_Position = projection * vPosition;		     \n"
    "}                                                       \n"; 

static const char gVertexShaderOffScreen[] =
    "#version 300 es                                         \n"
    "in vec4 vPosition;                                      \n"
    "in vec2 vTexCoords;                                     \n"
    "uniform mat4 projection;                                \n"

    "out vec2 yuvTexCoords;                                  \n"
    "void main() {                                           \n"
    "  yuvTexCoords = vTexCoords;                            \n"
    "  gl_Position = projection * vec4(vPosition.x, -vPosition.y, vPosition.zw);\n"
    "}                                                       \n";


static const char gFragmentShader_yuv_to_yuv[] =
    "#version 300 es                                         \n"
    "#extension GL_OES_EGL_image_external_essl3 : enable     \n"
    "#extension GL_EXT_YUV_target : enable                   \n"
    "precision mediump float;                                \n"
    "uniform __samplerExternal2DY2YEXT yuvTexSampler;        \n"
    "in vec2 yuvTexCoords;                                   \n"
    "layout (yuv) out vec4 color;                            \n"
    "void main() {                                           \n"
    "  color = texture(yuvTexSampler, yuvTexCoords);         \n"
    "}                                                       \n"; 

static const char gFragmentShader_yuv_to_rgb[] =
    "#version 300 es                                         \n"
    "#extension GL_OES_EGL_image_external_essl3 : enable     \n"
    "#extension GL_EXT_YUV_target : enable                   \n"
    "precision mediump float;                                \n"
    "uniform __samplerExternal2DY2YEXT yuvTexSampler;        \n"
    "in vec2 yuvTexCoords;                                   \n"
    "layout (yuv) out vec4 color;                            \n"
    "void main() {                                           \n"
    "  vec4 yuvTex = texture(yuvTexSampler, yuvTexCoords);   \n"
    "  vec3 rgbTex = yuv_2_rgb(yuvTex.xyz, itu_709);         \n"
    "  color = vec4(rgbTex, 255);                            \n"
    "}                                                       \n"; 

static const char gFragmentShader_rgb_to_yuv[] =
    "#version 300 es                                         \n"
    "#extension GL_OES_EGL_image_external_essl3 : enable     \n"
    "#extension GL_EXT_YUV_target : enable                   \n"
    "precision mediump float;                                \n"
    "uniform __samplerExternal2DY2YEXT yuvTexSampler;        \n"
    "in vec2 yuvTexCoords;                                   \n"
    "layout (yuv) out vec4 color;                            \n"
    "void main() {                                           \n"
    "  vec4 rgbTex = texture(yuvTexSampler, yuvTexCoords);   \n"
    "  vec3 yuvTex = rgb_2_yuv(rgbTex.xyz, itu_709);         \n"
    "  color =  vec4(yuvTex,0);                              \n"
    "}                                                       \n";

static const char gFragmentShader_rgb_to_rgb[] =
    "#version 300 es                                         \n"
    "#extension GL_OES_EGL_image_external_essl3 : enable     \n"
    "#extension GL_EXT_YUV_target : enable                   \n"
    "precision mediump float;                                \n"
    "uniform __samplerExternal2DY2YEXT yuvTexSampler;        \n"
    "in vec2 yuvTexCoords;                                   \n"
    "layout (yuv) out vec4 color;                            \n"
    "void main() {                                           \n"
    "  color =  texture(yuvTexSampler, yuvTexCoords);        \n"
    "}                                                       \n";


static const char *
eglErrorString(EGLint code)
{
#define MYERRCODE(x) case x: return #x;
	switch (code) {
	MYERRCODE(EGL_SUCCESS)
	MYERRCODE(EGL_NOT_INITIALIZED)
	MYERRCODE(EGL_BAD_ACCESS)
	MYERRCODE(EGL_BAD_ALLOC)
	MYERRCODE(EGL_BAD_ATTRIBUTE)
	MYERRCODE(EGL_BAD_CONTEXT)
	MYERRCODE(EGL_BAD_CONFIG)
	MYERRCODE(EGL_BAD_CURRENT_SURFACE)
	MYERRCODE(EGL_BAD_DISPLAY)
	MYERRCODE(EGL_BAD_SURFACE)
	MYERRCODE(EGL_BAD_MATCH)
	MYERRCODE(EGL_BAD_PARAMETER)
	MYERRCODE(EGL_BAD_NATIVE_PIXMAP)
	MYERRCODE(EGL_BAD_NATIVE_WINDOW)
	MYERRCODE(EGL_CONTEXT_LOST)
	default:
		return "unknown";
	}
#undef MYERRCODE
}

static int
checkEglErrorState(void)
{
	EGLint code;

	code = eglGetError();
	if (code != EGL_SUCCESS) {
		RE_ERR("EGL error state: %s (0x%04lx)\n",
			eglErrorString(code), (long)code);
		return -1;
	}

	return 0;
}

static const char *
glErrorString(EGLint code)
{
#define MYERRCODE(x) case x: return #x;
	switch (code) {
	MYERRCODE(GL_NO_ERROR);
	MYERRCODE(GL_INVALID_ENUM);
	MYERRCODE(GL_INVALID_VALUE);
	MYERRCODE(GL_INVALID_OPERATION);
	//MYERRCODE(GL_STACK_OVERFLOW_KHR);
	//MYERRCODE(GL_STACK_UNDERFLOW_KHR);
	MYERRCODE(GL_OUT_OF_MEMORY);
	default:
		return "unknown";
	}
#undef MYERRCODE
}

static bool
checkEglExtension(const char *extensions, const char *extension)
{
	size_t extlen = strlen(extension);
	const char *end = extensions + strlen(extensions);

	while (extensions < end) {
		size_t n = 0;

		/* Skip whitespaces, if any */
		if (*extensions == ' ') {
			extensions++;
			continue;
		}

		n = strcspn(extensions, " ");
		/* Compare strings */
		if (n == extlen && strncmp(extension, extensions, n) == 0)
			return true; /* Found */

		extensions += n;
	}

	/* Not found */
	return false;
}


static int checkGlErrorState(void)
{
	EGLint code;

	code = glGetError();
	if (code != GL_NO_ERROR) {
		RE_ERR("GL error state: %s (0x%04lx)\n",
			glErrorString(code), (long)code);
		return -1;
	}

	return 0;
}

static const char *getDrmFormatName(unsigned int format)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(DrmFormat); i++) {
		if (DrmFormat[i].drm_format == format) {
			return DrmFormat[i].name;
		}
	}

	return NULL;
}

static bool is_yuv_format(unsigned int format)
{
	switch (format) {
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_BGRA8888:
	case DRM_FORMAT_RGB888:
		return 0;
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV61:
	case DRM_FORMAT_NV16:
		return 1;
	}

	printf("ERROR, NOT Surpport this format:0x%x!\n", format);
	return 0;
}

static EGLImageKHR creatEglImage(EGLDisplay dpy, int dma_fd, unsigned int dataOffset,
		unsigned int width, unsigned int height, unsigned int format)
{
	int atti = 0;
	EGLint attribs0[30];

	//set the image's size
	attribs0[atti++] = EGL_WIDTH;
	attribs0[atti++] = width;
	attribs0[atti++] = EGL_HEIGHT;
	attribs0[atti++] = height;

	//set pixel format
	attribs0[atti++] = EGL_LINUX_DRM_FOURCC_EXT;
	attribs0[atti++] = format;

	switch (format) {
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_BGRA8888:
		attribs0[atti++] = EGL_DMA_BUF_PLANE0_FD_EXT;
		attribs0[atti++] = dma_fd;
		attribs0[atti++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
		attribs0[atti++] = dataOffset;
		attribs0[atti++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
		attribs0[atti++] = width * 4;
		break;
	case DRM_FORMAT_RGB888:
		attribs0[atti++] = EGL_DMA_BUF_PLANE0_FD_EXT;
		attribs0[atti++] = dma_fd;
		attribs0[atti++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
		attribs0[atti++] = dataOffset;
		attribs0[atti++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
		attribs0[atti++] = width * 3;
		break;
	case DRM_FORMAT_YUV420:
		//set buffer fd, offset, pitch for y component
		attribs0[atti++] = EGL_DMA_BUF_PLANE0_FD_EXT;
		attribs0[atti++] = dma_fd;
		attribs0[atti++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
		attribs0[atti++] = dataOffset;
		attribs0[atti++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
		attribs0[atti++] = width;

		//set buffer fd, offset ,pitch
		attribs0[atti++] = EGL_DMA_BUF_PLANE1_FD_EXT;
		attribs0[atti++] = dma_fd;
		attribs0[atti++] = EGL_DMA_BUF_PLANE1_OFFSET_EXT;
		attribs0[atti++] = dataOffset + width * height;
		attribs0[atti++] = EGL_DMA_BUF_PLANE1_PITCH_EXT;
		attribs0[atti++] = width / 2;

		//set buffer fd, offset ,pitch
		attribs0[atti++] = EGL_DMA_BUF_PLANE2_FD_EXT;
		attribs0[atti++] = dma_fd;
		attribs0[atti++] = EGL_DMA_BUF_PLANE2_OFFSET_EXT;
		attribs0[atti++] = dataOffset + width * height * 5 / 4;
		attribs0[atti++] = EGL_DMA_BUF_PLANE2_PITCH_EXT;
		attribs0[atti++] = width / 2;
		break;
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV61:
	case DRM_FORMAT_NV16:
		//set buffer fd, offset, pitch for y component
		attribs0[atti++] = EGL_DMA_BUF_PLANE0_FD_EXT;
		attribs0[atti++] = dma_fd;
		attribs0[atti++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
		attribs0[atti++] = dataOffset;
		attribs0[atti++] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
		attribs0[atti++] = width;

		//set buffer fd, offset ,pitch
		attribs0[atti++] = EGL_DMA_BUF_PLANE1_FD_EXT;
		attribs0[atti++] = dma_fd;
		attribs0[atti++] = EGL_DMA_BUF_PLANE1_OFFSET_EXT;
		attribs0[atti++] = dataOffset + width * height;
		attribs0[atti++] = EGL_DMA_BUF_PLANE1_PITCH_EXT;
		attribs0[atti++] = width;
		break;
	default:
		RE_ERR("format:%d NOT support\n", format);
	};

	//set color space and color range
	attribs0[atti++] = EGL_YUV_COLOR_SPACE_HINT_EXT;
	attribs0[atti++] = EGL_ITU_REC709_EXT;
	attribs0[atti++] = EGL_SAMPLE_RANGE_HINT_EXT;
	attribs0[atti++] = EGL_YUV_FULL_RANGE_EXT;

	attribs0[atti++] = EGL_NONE;

	RE_INF("creatEglImage:%dx%d dmafd:%d format:%s\n",
		width, height, dma_fd, getDrmFormatName(format));

	//Notice the target: EGL_LINUX_DMA_BUF_EXT
	return eglCreateImageKHR(dpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
				0, attribs0);
}

static EGLBoolean destroyEglImage(EGLDisplay dpy, EGLImageKHR image)
{
	return eglDestroyImageKHR(dpy, image);
}

int RenderEngine::setupTexture(int dmafd, unsigned int dataOffset,
		struct RE_Texture *retex,
		unsigned int width, unsigned int height, unsigned int format)
{
	GLuint Tex;
	EGLImageKHR img0;
	EGLDisplay dpy = (EGLDisplay)REDisplay;

	if (!retex) {
		RE_ERR("RE Texture is NULL!\n");
		return -1;
	}

	glActiveTexture(GL_TEXTURE0);

	img0 = creatEglImage(dpy, dmafd, dataOffset, width, height, format);
	if (img0 == EGL_NO_IMAGE_KHR) {
		checkEglErrorState();
		RE_ERR("creatEglImage failed\n");
		return -1;
	}
	retex->img = img0;

	//create gl texture and refered to EGL Image
	glGenTextures(1, &Tex);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES, Tex);

	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, (GLeglImageOES)img0);
	if (checkEglErrorState()) {
		RE_ERR("glEGLImageTargetTexture2DOES egl failed\n");
		return -1;
	}

	if (checkGlErrorState()) {
		RE_ERR("glEGLImageTargetTexture2DOES gles failed\n");
		return -1;
	}

	retex->tex = Tex;

	RE_INF("secceed to setupTexture!\n");

	return Tex;
}

int RenderEngine::destroyTexture(struct RE_Texture *retex)
{
	if (retex->tex)
		glDeleteTextures(1, &retex->tex);
	if (retex->img && (destroyEglImage((EGLDisplay)REDisplay,
			(EGLImageKHR)retex->img) == EGL_FALSE)) {
		RE_ERR("destroy tex egl image failed");
		return -1;
	}

	return 0;
}

int RenderEngine::createFBO(int dmafd, unsigned int dataOffset,
				       unsigned int format,
				       struct RE_Fbo *refbo)
{
	EGLImageKHR img0;
	GLuint Tex;
	EGLDisplay dpy = (EGLDisplay)REDisplay;

	glGenTextures(1, &Tex);
	glBindTexture(GL_TEXTURE_EXTERNAL_OES, Tex);

	img0 = creatEglImage(dpy, dmafd, dataOffset,
			     view_width, view_height,
			     format);
	if (img0 == EGL_NO_IMAGE_KHR) {
		checkEglErrorState();
		RE_ERR("creatEglImage failed\n");
		return -1;
	}
	REFbo.img = img0;

	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, (GLeglImageOES)img0);
	if (checkEglErrorState()) {
		RE_ERR("glEGLImageTargetTexture2DOES egl failed\n");
		return -1;
	}

	if (checkGlErrorState()) {
		RE_ERR("glEGLImageTargetTexture2DOES gl failed\n");
		return -1;
	}
	REFbo.tex = Tex;

	GLuint fbo;
	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);


	RE_INF("FBO image:%p Tex:%d\n", img0, Tex);

	//The texture target MUST be GL_TEXTURE_EXTERNAL_OES, NOT GL_TEXTURE_2D
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
					GL_TEXTURE_EXTERNAL_OES, Tex, 0);
	uint32_t glStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (glStatus != GL_FRAMEBUFFER_COMPLETE) {
		RE_ERR("glCheckFramebufferStatusOES error 0x%x", glStatus);
		return -1;
	}

	if (checkGlErrorState()) {
		RE_ERR("glFramebufferTexture2D GL failed\n");
		return -1;
	}

	glBindTexture(GL_TEXTURE_EXTERNAL_OES, 0);

	REFbo.fbo = fbo;
	RE_INF("succeed to create FBO\n");

	return fbo;
}

int RenderEngine::destroyFBO(struct RE_Fbo *refbo)
{
	glDeleteFramebuffers(1, &refbo->fbo);
	glDeleteTextures(1, &refbo->tex);
	if (destroyEglImage((EGLDisplay)REDisplay,
			(EGLImageKHR)refbo->img) == EGL_FALSE) {
		RE_ERR("destroy fbo egl image failed");
		return -1;
	}

	return 0;
}

static EGLint const config_attribute_list_pbuffer[] = {
	EGL_RED_SIZE, 8,
	EGL_GREEN_SIZE, 8,
	EGL_BLUE_SIZE, 8,
	EGL_ALPHA_SIZE, 8,
	EGL_BUFFER_SIZE, 32,

	EGL_STENCIL_SIZE, 0,
	EGL_DEPTH_SIZE, 24,

	EGL_SAMPLES, 4,

	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,

	EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
	EGL_NONE
};

static EGLint const config_attribute_list_window[] = {
	EGL_RED_SIZE, 8,
	EGL_GREEN_SIZE, 8,
	EGL_BLUE_SIZE, 8,
	EGL_ALPHA_SIZE, 8,
	EGL_BUFFER_SIZE, 32,

	EGL_STENCIL_SIZE, 0,
	EGL_DEPTH_SIZE, 24,

	EGL_SAMPLES, 4,

	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,

	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	EGL_NONE
};

static EGLint window_attribute_list[] = {
	EGL_NONE
};

static const EGLint context_attribute_list[] = {
	EGL_CONTEXT_CLIENT_VERSION, 3,
	EGL_NONE
};

int RenderEngine::initEGL(bool onScreen)
{
	int ret;
	EGLConfig config;
	EGLint num_config;
	const char *extensions;
	int max_num_config;
	int i, cfg_found = 0;

	REDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (REDisplay == EGL_NO_DISPLAY) {
		RE_ERR("Error: No display found!\n");
		return -1;
	}

	if (!eglInitialize(REDisplay, &egl_major, &egl_minor)) {
		RE_ERR("Error: eglInitialise failed!\n");
		return -1;
	}
	RE_INF("egl version:%d.%d\n", egl_major, egl_minor);

	extensions = eglQueryString(REDisplay, EGL_EXTENSIONS);
	RE_INF("EGL extension:%s\n", extensions);

	if (!checkEglExtension(extensions, "EGL_EXT_image_dma_buf_import")) {
		RE_ERR("egl extension image can NOT be imported by dma buffer\n");
		return -1;
	}

	if (!eglBindAPI (EGL_OPENGL_ES_API)) {
		RE_ERR("failed to bind api EGL_OPENGL_ES_API\n");
		return -1;
	}

	if (!eglGetConfigs(REDisplay, NULL, 0, &max_num_config)) {
		checkEglErrorState();
		RE_ERR("eglGetConfigs failed\n");
		return -1;
	}
	RE_INF("EGL max_num_config: %d\n", max_num_config);

	REConfig = malloc(sizeof(EGLConfig) * max_num_config);
	if (NULL == REConfig){
		RE_ERR("malloc for REConfig failed\n");
		return -1;
	} 

	if (onScreen)
		ret = eglChooseConfig(REDisplay, config_attribute_list_window,
			(EGLConfig *)REConfig, max_num_config, &num_config);
	else
		ret = eglChooseConfig(REDisplay, config_attribute_list_pbuffer,
			(EGLConfig *)REConfig, max_num_config, &num_config);
	if (!ret) {
		checkEglErrorState();
		RE_INF("eglChooseConfig failed!\n");
		return -1;
	}

	if (num_config == 0) {
		RE_INF("No matching egl config found!\n");
		return -1;
	}
	RE_INF("EGL has %d matching config\n", num_config);

	EGLConfig *configs = (EGLConfig *)REConfig;
	for ( i=0; i<num_config; i++ ) {
		EGLint value;
		/*Use this to explicitly check that
		the EGL config has the expected color depths */
		eglGetConfigAttrib( REDisplay, configs[i], EGL_RED_SIZE, &value );
		if (8 != value) continue;
		RE_INF("red OK: %d \n", value);
		eglGetConfigAttrib( REDisplay, configs[i], EGL_GREEN_SIZE, &value );
		if (8 != value) continue;
		RE_INF("green OK: %d \n", value);
		eglGetConfigAttrib( REDisplay, configs[i], EGL_BLUE_SIZE, &value );
		if (8 != value) continue;
		RE_INF("blue OK: %d \n", value);
		eglGetConfigAttrib( REDisplay, configs[i], EGL_ALPHA_SIZE, &value );
		if (8 != value) continue;
		RE_INF("alpha OK: %d \n", value);
		eglGetConfigAttrib( REDisplay, configs[i], EGL_SAMPLES, &value );
		if (4 != value) continue;
		RE_INF("fsaa OK: %d \n",value);

		config = configs[i];
		cfg_found=1;

		RE_INF("Found the EGL config, index:%d\n", i);
		break;
	}

	if (!cfg_found) {
		RE_ERR("No matching egl config found! that have configured\n");
		return -1;
	}

	if (onScreen) {
		RESurface = eglCreateWindowSurface(REDisplay, config,
				native_window,
				window_attribute_list);
		if (RESurface == EGL_NO_SURFACE) {
			checkEglErrorState();
			RE_ERR("eglCreateWindowSurface failed\n");
			return -1;
		}
	} else {
		RESurface = eglCreatePbufferSurface(REDisplay, config, NULL);
		if (RESurface == EGL_NO_SURFACE) {
			checkEglErrorState();
			RE_ERR("eglCreatePbufferSurface failed\n");
			return -1;
		}
	}

	REContext = eglCreateContext(REDisplay, config,
			EGL_NO_CONTEXT, context_attribute_list);
	if (REContext == EGL_NO_CONTEXT) {
		checkEglErrorState();
		RE_ERR("eglCreateContext failed\n");
		return -1;
	}

	if (!eglMakeCurrent(REDisplay, RESurface,
			    RESurface, REContext)) {
		checkEglErrorState();
		RE_ERR("eglMakeCurrent failed\n");
		return -1;
	}

	RE_INF("EGL Init Finish!\n\n\n");

	return 0;
}

int RenderEngine::exitEGL()
{
	if (eglDestroySurface((EGLDisplay)REDisplay,
			(EGLSurface)RESurface) == EGL_FALSE) {
		RE_ERR("eglDestroySurface failed\n");
		return -1;
	}

	if (eglDestroyContext((EGLDisplay)REDisplay,
		(EGLContext)REContext) == EGL_FALSE) {
		RE_ERR("eglDestroyContext failed\n");
		return -1;
	}

	free(REConfig);

	return 0;
}

int RenderEngine::initShader(const char *vertex, const char *fragment)
{
	REShaderProgram *sdpro = new REShaderProgram(vertex, fragment);
	shaderProgram = sdpro;
	return 0;
}

int RenderEngine::exitShader()
{
	delete (REShaderProgram *)shaderProgram;
	shaderProgram = NULL;

	return 0;
}

void setProjection(glm::mat4& Proj,
		unsigned int view_width, unsigned int view_height,
        unsigned int layer_width, unsigned int layer_height,
		float tX,         float tY,
		float scaleX,     float scaleY,
		float rotate,     unsigned int rotateType,
		bool on_screen)
{
	glm::mat4 projection = glm::ortho(-0.5f * view_width,
	                                   0.5f * view_width,
	                                   -0.5f * view_height,
	                                   0.5f * view_height,
	                                   0.1f, 2.0f);


	glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 1.0f),
				     glm::vec3(0.0f, 0.0f, 0.0f),
				     glm::vec3(0.0f, 1.0f, 0.0f));

	glm::mat4 model = glm::mat4(1.0f);

	model = glm::translate(model, glm::vec3(tX, tY, 0.0f));
	//model = glm::scale(model, glm::vec3(scaleX, scaleY, 1.0));

	if (on_screen) {
		model = glm::rotate(model,
				    glm::radians(rotate),
				    glm::vec3(0.0f, 0.0f, 1.0f));
		model = glm::scale(model, glm::vec3(scaleX, scaleY, 1.0));


		/*Translate the pic to the origin of coordinate*/
		if (rotateType == RE_ROTATE_ADAPTIVE_SCALE_WITH_LAYER_RATIO)
			    model = glm::translate(model, glm::vec3(-0.5f * layer_width,
								-0.5f * layer_height,
								 0.0f));            
		else
			    model = glm::translate(model, glm::vec3(-0.5f * view_width,
								-0.5f * view_height,
								 0.0f));
	} else {
		model = glm::rotate(model,
				    glm::radians(rotate),
				    glm::vec3(0.0f, 0.0f, -1.0f));
		model = glm::scale(model, glm::vec3(scaleX, scaleY, 1.0));

		/*Translate the pic to the origin of coordinate*/
		if (rotateType == RE_ROTATE_ADAPTIVE_SCALE_WITH_LAYER_RATIO)
			    model = glm::translate(model, glm::vec3(-0.5f * layer_width,
								 0.5f * layer_height,
								 0.0f));
		else
		    model = glm::translate(model, glm::vec3(-0.5f * view_width,
								 0.5f * view_height,
								 0.0f));
	}

	Proj = projection * view * model;
}


static float floatMin(float f1, float f2)
{
	if (f1 <= f2)
		return f1;
	return f2;
}

static float floatMin(float f1, float f2, float f3, float f4)
{
	if (f1 <= f2 && f1 <= f3 && f1 <= f4)
		return f1;
	if (f2 <= f1 && f2 <= f3 && f2 <= f4)
	    return f2;
    if (f3 <= f1 && f3 <= f2 && f3 <= f4)
        return f3;

    return f4;
}

static bool floatEqual(float f1, float f2)
{
	if(fabs(f1 - f2) < 1e-6)
		return true;
	return false;
}

float RenderEngine::getRotateScreenRatioAdaptiveScaleRate(
	float width, float height, float degree)
{
	float scale_rate1 = 1.0f, scale_rate2 = 1.0f;

	//set the coordinates of "up-right corner"
	//set the coordinates of "bottom-right corner"
	glm::vec4 right_up(width / 2.0f, height / 2.0f, 0.0f, 1.0f);
	glm::vec4 right_bottom(width / 2.0f, -height / 2.0f, 0.0f, 1.0f);

	glm::vec4 ru;
	glm::vec4 rb;

	glm::mat4 model = glm::mat4(1.0f);
	model = glm::rotate(model,
			    glm::radians(degree),
			    glm::vec3(0.0f, 0.0f, 1.0f));

	//calculate the coordinates of "up/bottom-right corner" after rotate
	ru = model * right_up;
	rb = model * right_bottom;

	if (width >= height) { //scale the after-rotate-heiht to the original-height
		if (fabs(ru.y) > (height / 2.0f))
			scale_rate1 = (height / 2.0f) / fabs(ru.y);
		if (fabs(rb.y) > (height / 2.0f))
			scale_rate2 = (height / 2.0f) / fabs(rb.y);
	} else { //scale the after-rotate-width to the original-width
		if (fabs(ru.x) > (width / 2.0f))
			scale_rate1 = (width / 2.0f) / fabs(ru.x);
		if (fabs(rb.x) > (width / 2.0f))
			scale_rate2 = (width / 2.0f) / fabs(rb.x);
	}

	return floatMin(scale_rate1, scale_rate2);
}

float RenderEngine::getRotateLayerRatioAdaptiveScaleRate(
	float screen_width, float screen_height,
    float layer_width, float layer_height,
    float degree)
{
	float scale_rate1 = 1.0f, scale_rate2 = 1.0f;
    float scale_rate3 = 1.0f, scale_rate4 = 1.0f;

	//set the coordinates of "up-right corner"
	//set the coordinates of "bottom-right corner"
	glm::vec4 right_up(layer_width / 2.0f, layer_height / 2.0f, 0.0f, 1.0f);
	glm::vec4 right_bottom(layer_width / 2.0f, -layer_height / 2.0f, 0.0f, 1.0f);

	glm::vec4 ru;
	glm::vec4 rb;

	glm::mat4 model = glm::mat4(1.0f);
	model = glm::rotate(model,
			    glm::radians(degree),
			    glm::vec3(0.0f, 0.0f, 1.0f));

	//calculate the coordinates of "up/bottom-right corner" after rotate
	ru = model * right_up;
	rb = model * right_bottom;

	scale_rate1 = (screen_height / 2.0f) / fabs(ru.y);
	scale_rate2 = (screen_height / 2.0f) / fabs(rb.y);
	scale_rate3 = (screen_width / 2.0f) / fabs(ru.x);
	scale_rate4 = (screen_width / 2.0f) / fabs(rb.x);

	return floatMin(scale_rate1, scale_rate2, scale_rate3, scale_rate4);
}

void RenderEngine::getRotateScaleRate(
            float view_width, float view_height,
			float layer_width, float layer_height,
            float rotateAngle, unsigned char rotateType,
			float *widthRate, float *heightRate)
{
	float rotateWidthScale, rotateHeightScale;

	if (rotateType == RE_ROTATE_NO_ADAPTIVE_SCALE) {
		rotateWidthScale = rotateHeightScale = 1.0f;
	} else if (rotateType == RE_ROTATE_ADAPTIVE_SCALE_WITH_SCREEN_RATIO) {
		rotateWidthScale = rotateHeightScale
			= getRotateScreenRatioAdaptiveScaleRate(
			view_width, view_height, rotateAngle);
	} else if (rotateType ==
        RE_ROTATE_ADAPTIVE_SCALE_WITH_SCREEN_RATIO_90_270_FULL_SCREEN) {
		if (floatEqual(fmod(rotateAngle, 360), 0.0f)
			|| floatEqual(fmod(rotateAngle, 180), 0.0f)) {
			rotateWidthScale = rotateHeightScale = 1.0f;
		} else if (floatEqual(fmod(rotateAngle, 270), 0.0f)
			|| floatEqual(fmod(rotateAngle, 90), 0.0f)) {
			rotateWidthScale = (1.0f * view_width) / (1.0f * view_height); 
			rotateHeightScale = (1.0f * view_height) / (1.0f * view_width);
		} else
			rotateWidthScale = rotateHeightScale
				= getRotateScreenRatioAdaptiveScaleRate(
				view_width, view_height, rotateAngle);
	} else if (rotateType == RE_ROTATE_ADAPTIVE_SCALE_WITH_LAYER_RATIO) {
        rotateWidthScale = rotateHeightScale =
			getRotateLayerRatioAdaptiveScaleRate(
			    view_width, view_height,
                layer_width, layer_height,rotateAngle);
    } else {
		rotateWidthScale = rotateHeightScale
			= getRotateScreenRatioAdaptiveScaleRate(
			view_width, view_height, rotateAngle);
	}

	*widthRate = rotateWidthScale;
	*heightRate = rotateHeightScale;
}

static int fbdevFd;
static struct fb_var_screeninfo fbdevVinfo;

RenderEngine::RenderEngine(bool onScreen, unsigned int srcFmt,
					unsigned int dstFmt)
	: view_width(0), view_height(0),
	  REDisplay(NULL), REContext(NULL), RESurface(NULL),
	  egl_major(0), egl_minor(0),
	  shaderProgram(NULL)

{
	int ret;
	const char *vertexSource, *fragmentSource;

	memset(&RETex, 0, sizeof(RETex));
	memset(&REFbo, 0, sizeof(REFbo));

	srcFormat = srcFmt;
	dstFormat = dstFmt;

	if (!fbdevFd && onScreen) {
		fbdevFd = open("/dev/fb0", O_RDWR);
		if (fbdevFd < 0) {
			RE_ERR("Failed to open /dev/fb0\n");
			return;
		}

		if (ioctl(fbdevFd, FBIOGET_VSCREENINFO, &fbdevVinfo)) {
			RE_ERR("Failed to get fb_var_screeninfo\n");
			return;
		}

		view_width = fbdevVinfo.xres;
		view_height = fbdevVinfo.yres;
	}

	REOnScreen = onScreen;
	ret = initEGL(onScreen);
	if (ret < 0) {
		RE_ERR("InitEGL failed\n");
		return;
	}

	if (onScreen)
		vertexSource = gVertexShaderOnScreen;
	else
		vertexSource = gVertexShaderOffScreen;

	if (!is_yuv_format(srcFormat) && !is_yuv_format(dstFormat))
		fragmentSource = gFragmentShader_rgb_to_rgb;
	else if (is_yuv_format(srcFormat) && !is_yuv_format(dstFormat))
		fragmentSource = gFragmentShader_yuv_to_rgb;
	else if (!is_yuv_format(srcFormat) && is_yuv_format(dstFormat))
		fragmentSource = gFragmentShader_rgb_to_yuv;
 	else
		fragmentSource = gFragmentShader_yuv_to_yuv;

	ret = initShader(vertexSource, fragmentSource);
	if (ret < 0) {
		exitEGL();
		RE_ERR("initShader failed\n");
		return;
	}

	((REShaderProgram *)shaderProgram)->use();
}

RenderEngine::~RenderEngine()
{
	destroyFBO(&REFbo);
	destroyTexture(&RETex);
	exitShader();
	exitEGL();
	close(fbdevFd);
	fbdevFd = 0;
	memset(&fbdevVinfo, 0, sizeof(fbdevVinfo));
}

void RenderEngine::REdump(void)
{
	RE_INF("------------------------ dump -------------------------\n");
	RE_INF("view_width:%u view_height:%u\n", view_width, view_height);

	RE_INF("display:%p context:%p surface:%p\n",
		REDisplay, REContext, RESurface);
	
	RE_INF("egl_version:%d.%d\n", egl_major, egl_minor);
	RE_INF("is onscreen:%d\n", REOnScreen);

	RE_INF("sdPro:%p\n", shaderProgram);

	RE_INF("tex:%d img:%p\n", RETex.tex, RETex.img);
	RE_INF("fbo:%d img:%p\n", REFbo.fbo, REFbo.img);
	RE_INF("------------------------ end -------------------------\n");
}

static float vVertices[4][2];
static float vTexVertices[4][2];

static void setVertexAttrib(unsigned int programId,
			    unsigned int tex_width, unsigned int tex_height,
			    unsigned int view_width, unsigned int view_height,
			    struct RE_rect *srcCrop,
			    unsigned int rotate_type)
{
	struct RE_rect crop;

	if (!srcCrop) {
		crop.left = 0;
		crop.top = 0;
		crop.right = tex_width;
		crop.bottom = tex_height;
	} else {
		memcpy(&crop, srcCrop, sizeof(crop));
	}

    if (rotate_type == RE_ROTATE_ADAPTIVE_SCALE_WITH_LAYER_RATIO) {
        vVertices[0][0] = 0.0f;
        vVertices[0][1] = (crop.bottom - crop.top) * 1.0f;

        vVertices[1][0] = (crop.right - crop.left) * 1.0f;
        vVertices[1][1] = (crop.bottom - crop.top) * 1.0f;

        vVertices[2][0] = (crop.right - crop.left) * 1.0f;
        vVertices[2][1] = 0.0f;

        vVertices[3][0] = 0.0f;
        vVertices[3][1] = 0.0f;
    } else {
        vVertices[0][0] = 0.0f;
        vVertices[0][1] = view_height * 1.0f;

        vVertices[1][0] = view_width * 1.0f;
        vVertices[1][1] = view_height * 1.0f;

        vVertices[2][0] = view_width * 1.0f;
        vVertices[2][1] = 0.0f;

        vVertices[3][0] = 0.0f;
        vVertices[3][1] = 0.0f;
    }

//Set all of the vertex information
	int vPosition = glGetAttribLocation(programId, "vPosition");
	glVertexAttribPointer(vPosition, 2, GL_FLOAT, GL_FALSE, 0, vVertices);
	glEnableVertexAttribArray(vPosition);

	vTexVertices[0][0] = (1.0f * crop.left) / (1.0f * tex_width);
	vTexVertices[0][1] = (1.0f * crop.top) / (1.0f * tex_height);

	vTexVertices[1][0] = (1.0f * crop.right) / (1.0f * tex_width);
	vTexVertices[1][1] = (1.0f * crop.top) / (1.0f * tex_height);

	vTexVertices[2][0] = (1.0f * crop.right) / (1.0f * tex_width);
	vTexVertices[2][1] = (1.0f * crop.bottom) / (1.0f * tex_height);

	vTexVertices[3][0] = (1.0f * crop.left) / (1.0f * tex_width);
	vTexVertices[3][1] = (1.0f * crop.bottom) / (1.0f * tex_height);

	RE_INF("TexCord: (%f, %f)  (%f, %f)  (%f, %f) (%f, %f)\n",
		vTexVertices[0][0], vTexVertices[0][1],
		vTexVertices[1][0], vTexVertices[1][1],
		vTexVertices[2][0], vTexVertices[2][1],
		vTexVertices[3][0], vTexVertices[3][1]);


	int vTex = glGetAttribLocation(programId, "vTexCoords");
	glVertexAttribPointer(vTex, 2, GL_FLOAT, GL_FALSE, 0, vTexVertices);
	glEnableVertexAttribArray(vTex);
}

int RenderEngine::drawLayerOffScreenCore(
			int srcFd, unsigned int srcDataOffset,
			unsigned int srcWidth, unsigned int srcHeight,
			struct RE_rect *srcCrop,
			float tX, float tY,
			float scaleX, float scaleY,
			float rotateAngle, unsigned char rotateType)
{
	int ret;
	float rotateWidthScale = 1.0f, rotateHeightScale = 1.0f;
	unsigned int tex_format = srcFormat;

	if (REOnScreen) {
		RE_ERR("is OnScreen RenderEngine\n");
		return -1;
	}

	RE_INF("Layer: %ux%u  fd:%d format:%s\n",
		srcWidth, srcHeight,srcFd,
		getDrmFormatName(tex_format));

	setVertexAttrib(((REShaderProgram *)shaderProgram)->getProgramId(),
		    srcWidth, srcHeight,
                    view_width, view_height,
                    srcCrop, rotateType);
	ret = setupTexture(srcFd, srcDataOffset, &RETex,
		     srcWidth, srcHeight, tex_format);
	if (ret < 0) {
		RE_ERR("setupTexture failed\n");
		return -1;
	}

	((REShaderProgram *)shaderProgram)->use();
	((REShaderProgram *)shaderProgram)->setInt("uTexSampler", 0);

	glBindTexture(GL_TEXTURE_EXTERNAL_OES, RETex.tex);

	//REdump();

	getRotateScaleRate(view_width, view_height,
			srcWidth,srcHeight,
            		rotateAngle, rotateType,
			&rotateWidthScale, &rotateHeightScale);

	glm::mat4 proj = glm::mat4(1.0f);
	setProjection(proj, view_width, view_height,
			srcWidth, srcHeight,
			tX, tY,
			rotateWidthScale * scaleX,
			rotateHeightScale * scaleY,
			rotateAngle, rotateType, 0);
	((REShaderProgram *)shaderProgram)->setMat4("projection", proj);

	checkGlErrorState();

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	return 0;
}

int RenderEngine::drawLayerOffScreenAtCentral(
		      unsigned char rotateType, void **sync)
{
	int ret;
	EGLSyncKHR egl_sync;

	if (RESf.size() > 1) {
		RE_ERR("current surfaceNum:%d, "
			"but drawLayerOffScreenAtCentral only surpport one surface!\n",
			(int)RESf.size());
	}

	ret = createFBO(dstDmaFd, dstDataOffset,
				dstFormat, &REFbo);
	if (ret < 0) {
		RE_ERR("createFBO failed\n");
		return -1;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, REFbo.fbo);

	glViewport(0, 0, view_width, view_height);
	if (is_yuv_format(dstFormat))
		glClearColor(0.0f, 0.5f, 0.5f, 1.0f);
	else
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);


	struct RESurface *surface = RESf[0];
	ret = drawLayerOffScreenCore(
			surface->srcFd,  surface->srcDataOffset,
			surface->srcWidth, surface->srcHeight,
			&surface->srcCrop,
			0.0f, 0.0f,
			1.0f, 1.0f,
			surface->rotateAngle, rotateType);



	if (ret < 0)
		return ret;

	egl_sync = eglCreateSyncKHR(REDisplay, EGL_SYNC_FENCE_KHR, NULL);
	if (EGL_NO_SYNC_KHR == egl_sync) {
		RE_ERR("eglCreateSyncKHR failed\n");
		return -1;
	}

	*sync = egl_sync;

	glFlush();

	return 0;
}

int RenderEngine::drawLayerOffScreen(void **sync)
{
	int ret;
	EGLSyncKHR egl_sync;

	int surfaceNum = RESf.size();
	if (surfaceNum < 1) {
		RE_ERR("No surface to draw\n");
		return -1;
	}

	// todo : fix mem leak for RETex
	if (surfaceNum > 1) {
		RE_ERR("drawLayerOffScreen too many surface to draw %d \n", surfaceNum);
		return -1;
	}

	ret = createFBO(dstDmaFd, dstDataOffset,
				dstFormat, &REFbo);
	if (ret < 0) {
		RE_ERR("createFBO failed\n");
		return -1;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, REFbo.fbo);

	glViewport(0, 0, view_width, view_height);
	if (is_yuv_format(dstFormat)) {
		RE_INF("is YUV format\n");
		glClearColor(0.0f, 0.5f, 0.5f, 1.0f);
	} else {
		RE_INF("is RGB format\n");
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	}
	glClear(GL_COLOR_BUFFER_BIT);


	for (int i = 0; i < surfaceNum; i++) {
		struct RESurface *surface = RESf[i];

		float finalTransX = 
			1.0f * ((surface->dstWinRect.right + surface->dstWinRect.left) / 2)
				- 1.0f * (view_width / 2);
		float finalTransY =
			-1.0f * ((surface->dstWinRect.bottom + surface->dstWinRect.top) / 2)
				+ 1.0f * (view_height / 2);
		float finalScaleX =
			((1.0f * (surface->dstWinRect.right - surface->dstWinRect.left))
				/ (1.0f * view_width));
		float finalScaleY =
			((1.0f * (surface->dstWinRect.bottom - surface->dstWinRect.top))
				/ (1.0f * view_height));

		ret = drawLayerOffScreenCore(
			surface->srcFd, surface->srcDataOffset,
			surface->srcWidth, surface->srcHeight,
			&surface->srcCrop,
			finalTransX, finalTransY,
			finalScaleX, finalScaleY,
			surface->rotateAngle, 0);
		if (ret < 0) {
			RE_ERR("drawLayerOffScreenCore failed! index:%d\n", i);
			continue;
		}
	}

	for(int i = 0; i < surfaceNum; i++) {
		RESf.pop_back();
	}

	egl_sync = eglCreateSyncKHR(REDisplay, EGL_SYNC_FENCE_KHR, NULL);
	if (EGL_NO_SYNC_KHR == egl_sync) {
		RE_ERR("eglCreateSyncKHR failed\n");
		return -1;
	}

	*sync = egl_sync;

	glFlush();
	return 0;
}

int RenderEngine::drawLayerOnScreenCore(
		int srcFd,
		unsigned int srcDataOffset,
		unsigned int srcWidth, unsigned int srcHeight,
		struct RE_rect *srcCrop,
		float tX, float tY,
		float scaleX, float scaleY,
		float rotateAngle, unsigned char rotateType)
{
	int ret;
 	float rotateWidthScale = 1.0f, rotateHeightScale = 1.0f;
	unsigned int tex_format = srcFormat;

	if (!REOnScreen) {
		RE_ERR("is NOT OnScreen RenderEngine\n");
		return -1;
	}

	RE_INF("Layer: %ux%u  fd:%d format:%s\n",
		srcWidth, srcHeight, srcFd,
		getDrmFormatName(tex_format));
	RE_INF("Crop:(%u, %u)~(%u, %u)\n", srcCrop->left, srcCrop->top,
					srcCrop->right, srcCrop->bottom);
	RE_INF("Tranlation:(%f, %f)\n", tX, tY);
	RE_INF("Scale:(%f,%f)\n", scaleX, scaleY);
	RE_INF("rotateAngle:%f\n", rotateAngle);
	RE_INF("rotaType:%d\n", rotateType);

	if (!fbdevFd || !fbdevVinfo.xres || !fbdevVinfo.yres) {
		RE_ERR("FBDEV has NOT ben open!\n");
		return -1;
	}

	setVertexAttrib(((REShaderProgram *)shaderProgram)->getProgramId(),
		srcWidth, srcHeight,
                view_width, view_height, srcCrop, rotateType);

	if (RETex.tex)
		destroyTexture(&RETex);
	ret = setupTexture(srcFd, srcDataOffset, &RETex,
		     srcWidth, srcHeight, tex_format);
	if (ret < 0) {
		RE_ERR("setupTexture failed\n");
		return -1;
	}
	RE_INF("\n\n\n");

	((REShaderProgram *)shaderProgram)->use();
	((REShaderProgram *)shaderProgram)->setInt("uTexSampler", 0);

	glBindTexture(GL_TEXTURE_EXTERNAL_OES, RETex.tex);

	REdump();

	if (rotateType)
		getRotateScaleRate(view_width, view_height,
            		srcCrop->right - srcCrop->left,
			srcCrop->bottom - srcCrop->top,
			rotateAngle, rotateType,
			&rotateWidthScale, &rotateHeightScale);

	glm::mat4 proj = glm::mat4(1.0f);
	setProjection(proj, view_width, view_height,
            		srcCrop->right - srcCrop->left,
			srcCrop->bottom - srcCrop->top,
			tX, tY,
			rotateWidthScale * scaleX,
			rotateHeightScale * scaleY,
			rotateAngle, rotateType, 1);
	((REShaderProgram *)shaderProgram)->setMat4("projection", proj);

	checkEglErrorState();
	checkGlErrorState();

	RE_INF("Start Drawing!\n");
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	checkGlErrorState();
	checkEglErrorState();

	return 0;
}

int RenderEngine::drawLayerOnScreenAtCentral(
		      unsigned char rotateType)
{
	int ret;

	if (RESf.size() > 1) {
		RE_ERR("current surfaceNum:%d, "
			"but drawLayerOnScreenAtCentral only surpport one surface!\n",
			(int)RESf.size());
	}
	glViewport(0, 0, fbdevVinfo.xres, fbdevVinfo.yres);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	struct RESurface *surface = RESf[0];

	ret = drawLayerOnScreenCore(
		surface->srcFd,
		surface->srcDataOffset,
		surface->srcWidth, surface->srcHeight,
		&surface->srcCrop,
		0.0f, 0.0f,
		1.0f, 1.0f,
		surface->rotateAngle, rotateType);
	if (ret < 0)
		return ret;

	RESf.pop_back();

	if (eglSwapBuffers(REDisplay, RESurface) == EGL_FALSE) {
		RE_ERR("eglSwapBuffers failed\n");
		checkEglErrorState();
		checkGlErrorState();
		return -1;
	}

	checkEglErrorState();
	checkGlErrorState();

	RE_INF("Drawing Finish!\n");
	return 0;
}

int RenderEngine::drawLayerOnScreen()
{
	int ret;
	int surfaceNum = RESf.size();

	if (surfaceNum <= 0) {
		RE_ERR("NO ant surface to draw!\n");
		return -1;
	}

	// todo : fix mem leak for RETex
	if (surfaceNum > 1) {
		RE_ERR("too many surface to draw\n");
		return -1;
	}

	glViewport(0, 0, fbdevVinfo.xres, fbdevVinfo.yres);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	for (int i = 0; i < surfaceNum; i++) {
		struct RESurface *surface = RESf[i];
		float finalTransX =
			1.0f * ((surface->dstWinRect.right + surface->dstWinRect.left) / 2) - 1.0f * (fbdevVinfo.xres / 2);
		float finalTransY =
			-1.0f * ((surface->dstWinRect.bottom + surface->dstWinRect.top) / 2) + 1.0f * (fbdevVinfo.yres / 2);
		float finalScaleX =
			((1.0f * (surface->dstWinRect.right - surface->dstWinRect.left)) / (1.0f * fbdevVinfo.xres));
		float finalScaleY =
			((1.0f * (surface->dstWinRect.bottom - surface->dstWinRect.top)) / (1.0f * fbdevVinfo.yres));

		ret = drawLayerOnScreenCore(
			surface->srcFd,
			surface->srcDataOffset,
			surface->srcWidth, surface->srcHeight,
			&surface->srcCrop,
			finalTransX, finalTransY,
			finalScaleX, finalScaleY,
			surface->rotateAngle, 0);
		if (ret < 0) {
			RE_ERR("drawLayerOnScreenCore failed! index:%d\n", i);
			continue;
		}

		printf("draw surface:%d\n", i);
	}

	for(int i = 0; i < surfaceNum; i++) {
		RESf.pop_back();
	}

	if (eglSwapBuffers(REDisplay, RESurface) == EGL_FALSE) {
		RE_ERR("eglSwapBuffers failed\n");
		checkEglErrorState();
		checkGlErrorState();
		return -1;
	}

	checkEglErrorState();
	checkGlErrorState();

	RE_INF("Drawing Finish!\n");
	return 0;
}

void RenderEngine::setOffScreenTarget(unsigned int screen_w,
				unsigned int screen_h,
				int dmaFd,
				unsigned int dataOffset)
{
	view_width = screen_w;
	view_height = screen_h;
	dstDmaFd = dmaFd;
	dstDataOffset = dataOffset;
}



void RenderEngine::setSurface(struct RESurface *surface)
{
	RESf.push_back(surface);
}

int RenderEngine::waitSync(void *sync)
{
	EGLint sync_info = EGL_TIMEOUT_EXPIRED_KHR;

	if (REOnScreen) {
		RE_ERR("is OnScreen RenderEngine, do NOT need sync!\n");
		return -1;
	}

	sync_info = eglClientWaitSyncKHR(REDisplay, sync,
			EGL_SYNC_FLUSH_COMMANDS_BIT_KHR, 3000000000);
	if (EGL_CONDITION_SATISFIED_KHR != sync_info) {
		RE_ERR("eglClientWaitSyncKHR timeout\n");
		return -1;
	}

	eglDestroySyncKHR(REDisplay, sync);

	destroyFBO(&REFbo);
	destroyTexture(&RETex);
	return 0;
}

RenderEngine_t renderEngineCreate(bool onScreen, unsigned char debug,
			unsigned int srcFormat, unsigned int dstFormat)
{
	printf("libsdk_render version:%s\n", MODULE_VERSION);
 	render_debug = debug;
 	RenderEngine *RE = new RenderEngine(onScreen, srcFormat, dstFormat);

	return (RenderEngine_t)RE;
}


void renderEngineDestroy(RenderEngine_t RE)
{
	delete(static_cast<RenderEngine *>(RE));
}

void renderEngineSetSurface(RenderEngine_t RE,
			struct RESurface *surface)
{
	RenderEngine *render = static_cast<RenderEngine *>(RE);

	render->setSurface(surface);
}

void renderEngineSetOffScreenTarget(RenderEngine_t RE,
				unsigned int screen_w,
				unsigned int screen_h,
				int dmaFd,
				unsigned int dataOffset)
{
	RenderEngine *render = static_cast<RenderEngine *>(RE);

	render->setOffScreenTarget(screen_w, screen_h, dmaFd, dataOffset);
}

int renderEngineDrawOnScreenAtCentral(RenderEngine_t RE,
	 unsigned char rotateType)
{
	RenderEngine *render = static_cast<RenderEngine *>(RE);

	return render->drawLayerOnScreenAtCentral(rotateType);
}

int renderEngineDrawOnScreen(RenderEngine_t RE)
{
	RenderEngine *render = static_cast<RenderEngine *>(RE);

	render->drawLayerOnScreen();
	return 0;
}

int renderEngineDrawOffScreenAtCentral(
		      RenderEngine_t RE,
		      unsigned char rotateType, void **sync)
{
	RenderEngine *render = static_cast<RenderEngine *>(RE);

	return render->drawLayerOffScreenAtCentral(rotateType, sync);
}

int renderEngineDrawOffScreen(
		      RenderEngine_t RE,
		      void **sync)
{
	RenderEngine *render = static_cast<RenderEngine *>(RE);

	return render->drawLayerOffScreen(sync);
}

int renderEngineWaitSync(RenderEngine_t RE, void *sync)
{
	RenderEngine *render = static_cast<RenderEngine *>(RE);

	return render->waitSync(sync);
}
