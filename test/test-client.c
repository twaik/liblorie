#include <unistd.h>
#include <lorie-client.h>
#include <lorie-renderer.h>
#include <lorie-log.h>

void lorie_renderer_create_X11_window(int sWindowWidth, int sWindowHeight, Display **dpy, Window *win, EGLDisplay *egldpy, EGLNativeWindowType *eglwin);
void lorie_renderer_x11_window_process_events(Display *dpy, struct lorie_client* c);

int loop(struct lorie_client *c, EGLDisplay *dpy) {
	int connected = lorie_client_is_connected(c);
	if (!connected) {
		if (lorie_client_connect(c)) {
			return 1;
		}
	} else {
		lorie_renderer_x11_window_process_events(dpy, c);
		if (lorie_client_sync(c, 0) == -1) 
			lorie_client_disconnect(c);
	}
	
	return 1;
}

int main(void) {
	
	Display *dpy;
	Window win;
	
	EGLDisplay eglDpy;
	EGLNativeWindowType eglWin;
	
	lorie_renderer_create_X11_window(960, 540, &dpy, &win, &eglDpy, &eglWin);
	
	struct lorie_client *c = lorie_client_create(eglDpy, eglWin);
	while (1) {
		if (!loop(c, dpy)) break;
		usleep(100);
	}
	lorie_client_destroy(&c);
	return 0;
}
