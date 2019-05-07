#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <lorie-client.h>
#include <lorie-renderer.h>
#include <lorie-log.h>
#include <lorie-client-protocol.h>
#define sAppName "Lorie test"

void lorie_renderer_create_X11_window(int sWindowWidth, int sWindowHeight, Display **dpy, Window *win, EGLDisplay *egldpy, EGLNativeWindowType *eglwin){
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
	if (success == EGL_FALSE) return;
	success = eglGetConfigs(display, NULL, 0, &numConfigs);
	if (success == EGL_FALSE) return;
	success = eglChooseConfig(display, configAttribs, &config, 1, &numConfigs);
	if (success == EGL_FALSE) return;

	context = eglCreateContext(display, config, EGL_NO_CONTEXT, NULL);
	if (context == EGL_NO_CONTEXT) return;
	
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
					 KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask;

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

void lorie_renderer_x11_window_process_events(Display *dpy, struct lorie_client* c){
    while (XPending(dpy))
    {
        XEvent e;
        XNextEvent(dpy, &e);

        switch (e.type)
        {
            case ButtonPress:
            {
				if (e.xbutton.type == ButtonPress)
				lorie_client_send_pointer_event(c, LORIE_INPUT_STATE_DOWN, e.xbutton.button, e.xbutton.x, e.xbutton.y);
				break;
            }
            case ButtonRelease:
            {
                if (e.xbutton.type == ButtonRelease)
				lorie_client_send_pointer_event(c, LORIE_INPUT_STATE_UP, e.xbutton.button, e.xbutton.x, e.xbutton.y);
                break;
            }
            case MotionNotify:
            {
				lorie_client_send_pointer_event(c, LORIE_INPUT_STATE_MOTION, 0, e.xbutton.x, e.xbutton.y);
                break;
            }
            case KeyPress:
            {
				lorie_client_send_key_event(c, LORIE_INPUT_STATE_DOWN, e.xkey.keycode);
                break;
            }
            case KeyRelease:
            {
				lorie_client_send_key_event(c, LORIE_INPUT_STATE_UP, e.xkey.keycode);
                break;
            }
            default:
            {
                break;
            }
        }
    }
}
