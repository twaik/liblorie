#define __USE_XOPEN 1
#define __USE_MISC 1

#include <math.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <sys/mman.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <assert.h>

#include <lorie-server.h>
#include <lorie-renderer.h>
#include <lorie-buffer.h>
#include <lorie-log.h>

#include <string.h>

#include <stdio.h>
#define M_PI 3.14159265358979323846

void DumpHex(const void* data, size_t size) {
	char ascii[17];
	size_t i, j;
	ascii[16] = '\0';
	for (i = 0; i < size; ++i) {
		printf("%02X ", ((unsigned char*)data)[i]);
		if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
			ascii[i % 16] = ((unsigned char*)data)[i];
		} else {
			ascii[i % 16] = '.';
		}
		if ((i+1) % 8 == 0 || i+1 == size) {
			printf(" ");
			if ((i+1) % 16 == 0) {
				printf("|  %s \n", ascii);
			} else if (i+1 == size) {
				ascii[(i+1) % 16] = '\0';
				if ((i+1) % 16 <= 8) {
					printf(" ");
				}
				for (j = (i+1) % 16; j < 16; ++j) {
					printf("   ");
				}
				printf("|  %s \n", ascii);
			}
		}
	}
}

static inline int memfd_create(const char *name, unsigned int flags) {
    return syscall(__NR_memfd_create, name, flags);
}

#define sWindowWidth 512
#define sWindowHeight 512
#define sAppName ""

void lorie_renderer_create_X11_window(Display **dpy, Window *win, EGLDisplay *egldpy, EGLNativeWindowType *eglwin){
    static const EGLint configAttribs[] =
    {
		EGL_RED_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_BLUE_SIZE, 8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 24,
		EGL_STENCIL_SIZE, 8,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_NONE
    };
    EGLBoolean success;
    EGLint numConfigs;
    
	Display* sDisplay;
	Window sWindow;
	
	EGLDisplay display;
	EGLConfig config;
	EGLContext context;
    
	sDisplay = XOpenDisplay(NULL);

	display = eglGetDisplay(sDisplay);
	success = eglInitialize(display, NULL, NULL);
	assert(success != EGL_FALSE);
	success = eglGetConfigs(display, NULL, 0, &numConfigs);
	assert(success != EGL_FALSE);
	success = eglChooseConfig(display, configAttribs, &config, 1, &numConfigs);
	assert(success != EGL_FALSE);

	context = eglCreateContext(display, config, EGL_NO_CONTEXT, NULL);
	assert(context != EGL_NO_CONTEXT);
	
	XSetWindowAttributes swa;
	XVisualInfo *vi, tmp;
	XSizeHints sh;
	int n;
	EGLint vid;

	eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &vid);
	tmp.visualid = vid;
	vi = XGetVisualInfo(sDisplay, VisualIDMask, &tmp, &n);
	swa.colormap = XCreateColormap(sDisplay, RootWindow(sDisplay, vi->screen), vi->visual, AllocNone);
	sh.flags = PMinSize | PMaxSize;
	sh.min_width = sh.max_width = sWindowWidth;
	sh.min_height = sh.max_height = sWindowHeight;
	swa.border_pixel = 0;
	swa.event_mask = ExposureMask | StructureNotifyMask | PointerMotionMask |
					 KeyPressMask | ButtonPressMask | ButtonReleaseMask;

	sWindow = XCreateWindow(sDisplay, RootWindow(sDisplay, vi->screen),
				0, 0, sWindowWidth, sWindowHeight, 0, vi->depth, InputOutput, vi->visual,
				CWBorderPixel | CWColormap | CWEventMask, &swa);
	XMapWindow(sDisplay, sWindow);
	XSetStandardProperties(sDisplay, sWindow, sAppName, sAppName,
						   None, (char **)0, 0, &sh);
    XFlush(sDisplay);
	
	*dpy = sDisplay;
	*win = sWindow;
	*egldpy = display;
	*eglwin = (EGLNativeWindowType) sWindow;
}

static void updateImage3(int width, int height, uint32_t *texDat){
    static int offset = 0;
    int i, j;

    for (i = 0; i < width; i++) for (j = 0; j < height; j++)
    {
        int r = 0, g = 0, b = 0;
        int x = (i+offset) % 0x100;
        switch ((i+offset) /256 % 6)
        {
            case 0: r = 255; g = x;       break;
            case 1: g = 255; r = 255 - x; break;
            case 2: g = 255; b = x;       break;
            case 3: b = 255; g = 255 - x; break;
            case 4: b = 255; r = x;       break;
            case 5: r = 255; b = 255 - x; break;
            default:break;
        }
        texDat [ i + j*width ] = (uint32_t) ((r << 16) | (g << 8) | (b << 0));
    }
    offset+=10;
}

int main(void) {
	Display *dpy;
	Window win;
	
	EGLDisplay eglDpy;
	EGLNativeWindowType eglWin;
	
	lorie_renderer_create_X11_window(&dpy, &win, &eglDpy, &eglWin);
	struct lorie_renderer *r = lorie_renderer_create(eglDpy, eglWin);
	if (r == NULL) {
		LOGF ("r == NULL: true\n");
		return -1;
	}
    
    struct lorie_buffer *b = lorie_buffer_fd_create(sWindowWidth, sWindowHeight);
    lorie_renderer_buffer_attach(r, lorie_buffer_fd_fd(b), lorie_buffer_fd_width(b), lorie_buffer_fd_height(b));
    
    for (int i=0; i<200; i++) {
		updateImage3(lorie_buffer_fd_width(b), lorie_buffer_fd_height(b), lorie_buffer_fd_data(b));
		lorie_renderer_damage_surface(r,0,0,0,0);
		usleep(10000);
	}
    
    lorie_buffer_fd_destroy(&b);
    lorie_renderer_destroy(&r);
    return 0;
}
