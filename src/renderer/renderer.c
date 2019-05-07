#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <sys/mman.h>

#include <lorie-log.h>
#include "lorie-renderer.h"

#include "gl.c"
#ifdef DBG
#undef DBG
#endif
#define DBG LOGD("Here: %d", __LINE__)
	
static const EGLint attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_BLUE_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_RED_SIZE, 8,
		EGL_NONE
};
static EGLint ctxattr[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
};

struct lorie_renderer* lorie_renderer_create(EGLDisplay display, EGLNativeWindowType window) {
	EGLint numConfigs;
	EGLConfig config;
	
	if (window == 0) return NULL;
	#ifndef __ANDROID__
	if (display == NULL) return NULL;
	#else
	display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	#endif // __ANDROID__
	
	struct lorie_renderer *r = calloc(1, sizeof(struct lorie_renderer));
	if (r == NULL) {
		LOGE("Can not allocate struct lorie_renderer");
		goto fail;
	}
	
	r->dpy = display;
	
	r->ctx = EGL_NO_CONTEXT;
	r->sfc = EGL_NO_SURFACE;
	
	eglInitialize(r->dpy, 0, 0);	
	eglChooseConfig(r->dpy, attribs, &config, 1, &numConfigs);
	
	r->sfc = eglCreateWindowSurface(r->dpy, config, window, NULL);
	r->ctx = eglCreateContext(r->dpy, config, NULL, ctxattr);
	if (eglMakeCurrent(r->dpy, r->sfc, r->sfc, r->ctx) == EGL_FALSE) {
		checkEGLError();
		LOGE("GLESv2: Unable to eglMakeCurrent");
		goto fail;
	}
	
	// EGL setting up done. Initializing GLESv2.
	
	glGenTextures(1, &r->rootTex);
	
	r->gTextureProgram = createProgram(gSimpleVS, gSimpleFS); checkGlError("glCreateProgram");
	if (!r->gTextureProgram) {
		LOGE("GLESv2: Unable to create shader program");
		goto fail;
	}
	r->gvPos = (GLuint) glGetAttribLocation(r->gTextureProgram, "position"); checkGlError("glGetAttribLocation");
	r->gvCoords = (GLuint) glGetAttribLocation(r->gTextureProgram, "texCoords"); checkGlError("glGetAttribLocation");
	r->gvTextureSamplerHandle = (GLuint) glGetUniformLocation(r->gTextureProgram, "texture"); checkGlError("glGetAttribLocation");

	// GL is initialized. Clearing the screen.

	glClearColor(0.f, 0.f, 0.f, 0.f);
	glClear(GL_COLOR_BUFFER_BIT);
	eglSwapBuffers(r->dpy, r->sfc);
	eglMakeCurrent(r->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	return r;

fail:
	eglMakeCurrent(r->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	if (r != NULL) {
		lorie_renderer_destroy(&r);
	}
	return NULL;
}

void lorie_renderer_attach_window(struct lorie_renderer* r, EGLNativeWindowType win) {
	if (r == NULL || r->dpy == EGL_NO_DISPLAY) return;

	lorie_renderer_buffer_attach(r, 0, 0, 0);
	eglMakeCurrent(r->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	if (r->sfc != EGL_NO_SURFACE) eglDestroySurface(r->dpy, r->sfc);
	r->sfc = EGL_NO_SURFACE;
	
	if (win == NULL) return;
	
	EGLint numConfigs;
	EGLConfig config;
	eglChooseConfig(r->dpy, attribs, &config, 1, &numConfigs);
	
	r->sfc = eglCreateWindowSurface(r->dpy, config, win, NULL);
	checkEGLError();
}

void lorie_renderer_get_resolution(struct lorie_renderer *r, int32_t *w, int32_t *h) {
	if (r == NULL) return;
	if (w != NULL) {
		eglQuerySurface(r->dpy, r->sfc, EGL_WIDTH, w);
	}
	if (h != NULL) {
		eglQuerySurface(r->dpy, r->sfc, EGL_HEIGHT, h);
	}
}

static void glDrawTexCoords(struct lorie_renderer* r, float x0, float y0, float x1, float y1) {
	if (r == NULL) return;
	float coords[20] = {
			x0, -y0, 0.f, 0.f, 0.f,
			x1, -y0, 0.f, 1.f, 0.f,
			x0, -y1, 0.f, 0.f, 1.f,
			x1, -y1, 0.f, 1.f, 1.f
	};

	glUseProgram(r->gTextureProgram); checkGlError("glUseProgram");

	glVertexAttribPointer(r->gvPos, 3, GL_FLOAT, GL_FALSE, TRIANGLE_VERTICES_DATA_STRIDE_BYTES, coords); checkGlError("glVertexAttribPointer");
	glVertexAttribPointer(r->gvCoords, 2, GL_FLOAT, GL_FALSE, TRIANGLE_VERTICES_DATA_STRIDE_BYTES, &coords[3]); checkGlError("glVertexAttribPointer");
	glEnableVertexAttribArray(r->gvPos); checkGlError("glEnableVertexAttribArray");
	glEnableVertexAttribArray(r->gvCoords); checkGlError("glEnableVertexAttribArray");
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); checkGlError("glDrawArrays");

	glUseProgram(0); checkGlError("glUseProgram");
}

void lorie_renderer_damage_surface(struct lorie_renderer* r, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
	if (r == NULL || r->sfc == EGL_NO_SURFACE) return;
	if (lorie_buffer_fd_data(r->surface) == NULL) return;
	//if ((x1-x0)<=0 || (y1-y0) <= 0) return;
	int y;
	int32_t *data = lorie_buffer_fd_data(r->surface);
	int32_t *offset = &data[lorie_buffer_fd_width(r->surface) * y0 * sizeof(int32_t)];

	eglMakeCurrent(r->dpy, r->sfc, r->sfc, r->ctx);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, r->rootTex);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, lorie_buffer_fd_width(r->surface), lorie_buffer_fd_height(r->surface), GL_RGBA, GL_UNSIGNED_BYTE, lorie_buffer_fd_data(r->surface));
	//for (y=y0; y<y1; y++)
	//	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, lorie_buffer_fd_width(r->surface), y, GL_RGBA, GL_UNSIGNED_BYTE, offset);
	checkGlError("glTexSubImage2D");
	glDrawTexCoords(r, -1.f, -1.f, 1.0f, 1.0f);

	eglSwapBuffers(r->dpy, r->sfc);
	eglMakeCurrent(r->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

void lorie_renderer_buffer_attach(struct lorie_renderer *r, int32_t fd, int32_t width, int32_t height) {
	if (r == NULL) return;
	if (r->surface != NULL) {
		// Buffer is already mapped. Unmap first.
		lorie_buffer_fd_destroy(&r->surface);
	}
	
	if (fd < 0 || width <= 0 || height <= 0) return;
	
	r->surface = lorie_buffer_fd_from_received(fd, width, height);
	if (r->surface == NULL) {
		LOGE("Failed to attach lorie_buffer got from server");
		return;
	}
	
	void *data = lorie_buffer_fd_data(r->surface);
	if (data == MAP_FAILED) {
		return;
	}
	
	eglMakeCurrent(r->dpy, r->sfc, r->sfc, r->ctx);
	glViewport(0, 0, width, height);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, r->rootTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, lorie_buffer_fd_width(r->surface), lorie_buffer_fd_height(r->surface), 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	checkGlError("glTexImage2D");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	eglMakeCurrent(r->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

void lorie_renderer_destroy(struct lorie_renderer** renderer) {
	if (renderer == NULL || *renderer == NULL) return;
	struct lorie_renderer *r = *renderer;
	
	lorie_renderer_buffer_attach(r, 0, 0, 0);
	
	if (r->sfc != EGL_NO_SURFACE) eglDestroySurface(r->dpy, r->sfc);
	if (r->ctx != EGL_NO_CONTEXT) eglDestroyContext(r->dpy, r->ctx);
	
	free(*renderer);
	*renderer = NULL;
}
