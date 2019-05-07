#pragma once
#include <stdint.h>
#include <EGL/egl.h>

struct lorie_client;

struct lorie_client_callbacks {
	void (*buffer_change_fd)(void *data, int32_t fd, int32_t w, int32_t h);
	void (*damage)(void *data, int32_t x, int32_t y, int32_t w, int32_t h);
};

//struct lorie_client *lorie_client_create(struct lorie_client_callbacks* callbacks, void *callback_data);
struct lorie_client *lorie_client_create(EGLDisplay dpy, EGLNativeWindowType window);

void lorie_client_attach_window(struct lorie_client *c, EGLNativeWindowType w);
void lorie_client_get_resolution(struct lorie_client *c, int32_t *w, int32_t *h);

int lorie_client_connect(struct lorie_client* c);
int lorie_client_is_connected(struct lorie_client *c);
int lorie_client_sync(struct lorie_client* c, int block);

void lorie_client_send_pointer_event(struct lorie_client* c, int32_t state, int32_t button, int32_t x, int32_t y);
void lorie_client_send_key_event(struct lorie_client* c, int32_t state, int32_t key);
void lorie_client_send_get_current_buffer_request(struct lorie_client *c);
void lorie_client_send_resolution_change_request(struct lorie_client *c, int32_t x, int32_t y);

void lorie_client_disconnect(struct lorie_client* c);
void lorie_client_destroy(struct lorie_client **client);
