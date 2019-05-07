#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include <wayland-client.h>
#include <lorie-client-protocol.h>

#include <lorie-client.h>
#include <lorie-renderer.h>
#include <lorie-log.h>

struct lorie_client {
	struct wl_display *display;
	void *input;
	void *output;
	struct lorie_renderer *renderer;
	int32_t damage_rect[4];
};

static void handle_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version) {
	struct lorie_client *lorie_client = data;
	if (strcmp(interface, "lorie_input") == 0) {
		lorie_client->input = wl_registry_bind(registry, name,
			&lorie_input_interface, 1);
	} else if (strcmp(interface, "lorie_output") == 0) {
		lorie_client->output = wl_registry_bind(registry, name,
			&lorie_output_interface, 1);
	}
}

static void handle_global_remove(void *data, struct wl_registry *registry,
		uint32_t name) {
	// Who cares?
}

static const struct wl_registry_listener registry_listener = {
	.global = handle_global,
	.global_remove = handle_global_remove,
};

static void lorie_buffer_change_fd_handler(void *data, struct lorie_output *lorie_output, int32_t fd, int32_t w, int32_t h) {
	struct lorie_client* c = data;
	if (c  == NULL || c->renderer == NULL) return;
	struct lorie_renderer *r = c->renderer;
	
	lorie_renderer_buffer_attach(r, fd, w, h);
	c->damage_rect[0] = 0;
	c->damage_rect[1] = 0;
	c->damage_rect[2] = w;
	c->damage_rect[3] = h;
	lorie_renderer_damage_surface(c->renderer, 0, 0, 0, 0);
}

static void lorie_damage_handler(void *data, struct lorie_output *lorie_output, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
	struct lorie_client* c = data;
	if (c  == NULL || c->renderer == NULL) return;
	c->damage_rect[0] = x0;
	c->damage_rect[1] = y0;
	c->damage_rect[2] = x1;
	c->damage_rect[3] = y1;
	
	lorie_renderer_damage_surface(c->renderer, 0, 0, 0, 0);
}

struct lorie_output_listener lorie_output_listener = {
	lorie_buffer_change_fd_handler,
	lorie_damage_handler
};

struct lorie_client *lorie_client_create(EGLDisplay dpy, EGLNativeWindowType window) {
	struct lorie_client *c = calloc(1, sizeof(struct lorie_client));
	if (c == NULL) {
		LOGE("Failed allocating lorie_client: No memory (%m)\n");
	}

	c->renderer = lorie_renderer_create(dpy, window);
	if (c->renderer == NULL) {DBG;
		LOGF ("Creating Lorie renderer failed");
		free(c);
		return NULL;
	}

	return c;
}

int lorie_client_is_connected(struct lorie_client *c) {
	if (c == NULL) return 0;
	return c->display != NULL;
}

int lorie_client_connect(struct lorie_client* c) {
	if (c == NULL) return -1;
	
	c->display = wl_display_connect("lorie");
	if (c->display == NULL) {
		LOGE("Failed to connect to Wayland display: %m\n");
		return -1;
	}
	
	LOGI("Current connection fd is %d", wl_display_get_fd(c->display));
	
	struct wl_registry *registry = wl_display_get_registry(c->display);
	wl_registry_add_listener(registry, &registry_listener, c);
	wl_display_roundtrip(c->display);
	
	lorie_output_add_listener(c->output, &lorie_output_listener, c);
	
	if (c->input == NULL || c->output == NULL) {
		LOGE("Failed registering lorie_input\n"); 
		return -1;
	}
	
	if (c->renderer != NULL) {
		int32_t w, h;
		lorie_renderer_get_resolution(c->renderer, &w, &h);
		lorie_client_send_resolution_change_request(c, w, h);
		LOGI("Got resolution %dx%d\n", w, h);
	}
	return 0;
}

void lorie_client_disconnect(struct lorie_client* c) {
	if (c == NULL || c->display == NULL) return;
	wl_display_disconnect(c->display);
	c->display = NULL;
}

static int bytesAvailable(int sockfd) {
    int bytes = 0;
    if (ioctl(sockfd, FIONREAD, &bytes) == -1) {
        LOGE("[%d][%s] ioctl failed.", sockfd, __PRETTY_FUNCTION__);
        exit(1);
	}
	
    return bytes;
}

int lorie_client_sync(struct lorie_client* c, int block) {
	if (c == NULL) return -1;
	struct wl_display *display = c->display;
	int ret = -1;
	if (bytesAvailable(wl_display_get_fd(display)) > 0 || block) {
		ret = wl_display_dispatch(display);
	} else {
		ret = wl_display_flush(display);
	}
	
	if (c->renderer == NULL) return ret;
	struct lorie_renderer* r = c->renderer;
	lorie_renderer_damage_surface(r, c->damage_rect[0], c->damage_rect[1], c->damage_rect[2], c->damage_rect[3]);
	c->damage_rect[0] = c->damage_rect[1] = c->damage_rect[2] = c->damage_rect[3] = 0;

	return ret;
}

void lorie_client_send_pointer_event(struct lorie_client* c, int32_t state, int32_t button, int32_t x, int32_t y) {
	if (c == NULL) return;
	
	lorie_input_pointer(c->input, state, button, x, y);
}

void lorie_client_send_key_event(struct lorie_client* c, int32_t state, int32_t key) {
	if (c == NULL) return;
	
	lorie_input_key(c->input, state, key);
}

void lorie_client_send_get_current_buffer_request(struct lorie_client *c) {
	if (c == NULL) return;
	
	lorie_output_get_current_buffer(c->output);
}

void lorie_client_send_resolution_change_request(struct lorie_client *c, int32_t x, int32_t y) {
	if (c == NULL) return;
	
	lorie_output_resolution_change(c->output, x, y);
}

void lorie_client_attach_window(struct lorie_client *c, EGLNativeWindowType w) {
	if (c == NULL || c->renderer == NULL) return;
	
	lorie_renderer_attach_window(c->renderer, w);
}

void lorie_client_get_resolution(struct lorie_client *c, int32_t *w, int32_t *h) {
	if (c == NULL || c->renderer == NULL) return;
	
	lorie_renderer_get_resolution(c->renderer, w, h);
}

void lorie_client_destroy(struct lorie_client **client) {
	if (client == NULL) return;
	struct lorie_client *c = *client;
	if (c != NULL) {
		lorie_client_disconnect(c);
		free(c);
	}
	*client = NULL;
}
