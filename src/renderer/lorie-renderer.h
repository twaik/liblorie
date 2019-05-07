#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include <lorie-buffer.h>

struct lorie_renderer {
	EGLDisplay dpy;
	EGLContext ctx;
	EGLSurface sfc;

    GLuint rootTex;
    GLuint gTextureProgram;
    GLuint gvPos;
    GLuint gvCoords;
    GLuint gvTextureSamplerHandle;
    
    struct lorie_buffer* surface;
};

struct lorie_renderer* lorie_renderer_create(EGLDisplay display, EGLNativeWindowType window);

void lorie_renderer_attach_window(struct lorie_renderer* r, EGLNativeWindowType win);
void lorie_renderer_buffer_attach(struct lorie_renderer *r, int32_t fd, int32_t width, int32_t height);

void lorie_renderer_get_resolution(struct lorie_renderer *r, int32_t *w, int32_t *h);
void lorie_renderer_damage_surface(struct lorie_renderer* r, int32_t x0, int32_t y0, int32_t x1, int32_t y1);
void lorie_renderer_destroy(struct lorie_renderer** renderer);
