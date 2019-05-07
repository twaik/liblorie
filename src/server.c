#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>

#include <wayland-server.h>
#include <lorie-server-protocol.h>

#include <lorie-server.h>
#include <lorie-buffer.h>
#include <lorie-log.h>

struct lorie_server {
	int server_fd;
	struct wl_display *display;
	struct wl_global *lorie_input_global;
	struct wl_global *lorie_output_global;
	struct wl_resource *lorie_input_implementation;
	struct wl_resource *lorie_output_implementation;
	
	struct lorie_buffer *surface;
	
	struct wl_listener new_client_listener;
	
	struct lorie_server_callbacks *callbacks;
	void *callback_data;
};

struct lorie_server_client {
	struct lorie_server *server;
	struct wl_listener client_destroyed_listener;
};

static void lorie_pointer_event_handler(struct wl_client *client, struct wl_resource *resource, int32_t state, int32_t button, int32_t x, int32_t y) {
	struct lorie_server *s = resource->data;
	
	if (s != NULL && s->callbacks != NULL && s->callbacks->pointer_event != NULL)
		s->callbacks->pointer_event(s->callback_data, state, button, x, y);
}

static void lorie_key_event_handler(struct wl_client *client, struct wl_resource *resource, int32_t state, int32_t key) {
	struct lorie_server *s = resource->data;
	
	if (s != NULL && s->callbacks != NULL && s->callbacks->key_event != NULL)
		s->callbacks->key_event(s->callback_data, state, key);
}

static const struct lorie_input_interface lorie_input_impl = {
	lorie_pointer_event_handler,
	lorie_key_event_handler
};

static void lorie_get_current_buffer_handler(struct wl_client *client, struct wl_resource *resource) {
	struct lorie_server *s = resource->data;
	
	if (s != NULL  && !wl_list_empty(wl_display_get_client_list(s->display)))
		lorie_output_send_buffer_change_fd(
			s->lorie_output_implementation,
			lorie_buffer_fd_fd(s->surface),
			lorie_buffer_fd_width(s->surface),
			lorie_buffer_fd_height(s->surface) );
}

static void lorie_resolution_change_handler(struct wl_client *client, struct wl_resource *resource, int32_t x, int32_t y) {
	struct lorie_server *s = resource->data;
	
	if (s != NULL && s->callbacks != NULL && s->callbacks->resolution_change != NULL)
		s->callbacks->resolution_change(s->callback_data, x, y);
}

static const struct lorie_output_interface lorie_output_impl = {
	lorie_get_current_buffer_handler,
	lorie_resolution_change_handler
};

static void lorie_input_bind(struct wl_client *wl_client, void *data,
		uint32_t version, uint32_t id) {
	struct lorie_server *s = data;

	struct wl_resource *wl_resource = wl_resource_create(wl_client, &lorie_input_interface, version, id);
	if (wl_resource == NULL) {
		wl_client_post_no_memory(wl_client);
		return;
	}
	wl_resource_set_implementation(wl_resource, &lorie_input_impl, s, NULL);
	s->lorie_input_implementation = wl_resource;
}

static void lorie_output_bind(struct wl_client *wl_client, void *data,
		uint32_t version, uint32_t id) {
	struct lorie_server *s = data;
	
	struct wl_resource *wl_resource = wl_resource_create(wl_client, &lorie_output_interface, version, id);
	if (wl_resource == NULL) {
		wl_client_post_no_memory(wl_client);
		return;
	}
	wl_resource_set_implementation(wl_resource, &lorie_output_impl, s, NULL);
	s->lorie_output_implementation = wl_resource;
}

static void lorie_display_client_destroyed_handler(struct wl_listener *listener, void *data) {
	struct wl_client* c = data;
	struct lorie_server_client *lc = wl_container_of(listener, lc, client_destroyed_listener);
	if (lc == NULL || lc->server == NULL) {
		LOGE("lc = %p; lc->s = %p", lc, lc->server);
		return;
	}
	
	struct lorie_server *s = lc->server;
	if (s != NULL && s->callbacks != NULL && s->callbacks->client_destroyed != NULL) {
		s->callbacks->client_destroyed(s->callback_data, wl_client_get_fd(c));
		
		free(lc);
	}
}

static void lorie_display_client_created_handler(struct wl_listener *listener, void *data) {
	struct wl_client* c = data;
	struct lorie_server *s = wl_container_of(listener, s, new_client_listener);
	if (s != NULL && s->callbacks != NULL && s->callbacks->client_created != NULL) {
		s->callbacks->client_created(s->callback_data, wl_client_get_fd(c));
	
		if (s->callbacks->client_destroyed != NULL) {
			struct lorie_server_client* lc = calloc(1, sizeof(struct lorie_server_client));
			if (lc == NULL) {
				LOGE("Alocating lorie_server_client failed: %m");
				return;
			}
			
			wl_client_add_destroy_listener(c, &lc->client_destroyed_listener);
			lc->client_destroyed_listener.notify = lorie_display_client_destroyed_handler;
			lc->server = s;
		}
	}
}

static int set_cloexec_or_close(int fd)
{
	long flags;

	if (fd == -1)
		return -1;

	flags = fcntl(fd, F_GETFD);
	if (flags == -1)
		goto err;

	if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1)
		goto err;

	return fd;

err:
	close(fd);
	return -1;
}

static int
wl_os_socket_cloexec(int domain, int type, int protocol)
{
	int fd;

	fd = socket(domain, type | SOCK_CLOEXEC, protocol);
	if (fd >= 0)
		return fd;
	if (errno != EINVAL)
		return -1;

	fd = socket(domain, type, protocol);
	return set_cloexec_or_close(fd);
}

// Needed to track server socket from Xorg's SetNotifyFD
static int lorie_create_server_socket() {
	const char *name = "lorie";
	int name_size;
	const char *runtime_dir;
	socklen_t size;
	struct sockaddr_un addr;
	int fd;

	runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (!runtime_dir) {
		LOGE("error: XDG_RUNTIME_DIR not set in the environment\n");

		/* to prevent programs reporting
		 * "failed to add socket: Success" */
		errno = ENOENT;
		return -1;
	}

	addr.sun_family = AF_LOCAL;
	name_size = snprintf(addr.sun_path, sizeof addr.sun_path, "%s/%s", runtime_dir, name) + 1;

	if (name_size <= 0) {
		LOGE("Invalid socket name size!");
		return -1;
	}
	if (name_size > (int)sizeof addr.sun_path) {
		LOGE("error: socket path \"%s/%s\" plus null terminator"
		       " exceeds 108 bytes\n", runtime_dir, name);
		*addr.sun_path = 0;
		/* to prevent programs reporting
		 * "failed to add socket: Success" */
		errno = ENAMETOOLONG;
		return -1;
	}

	fd = wl_os_socket_cloexec(PF_LOCAL, SOCK_STREAM, 0);
	if (fd < 0) {
		return -1;
	}

	unlink(addr.sun_path);

	size = offsetof (struct sockaddr_un, sun_path) + strlen(addr.sun_path);
	if (bind(fd, (struct sockaddr *) &addr, size) < 0) {
		LOGE("bind() failed with error: %m\n");
		return -1;
	}

	if (listen(fd, 128) < 0) {
		LOGE("listen() failed with error: %m\n");
		return -1;
	}

	return fd;
}

struct lorie_server* lorie_server_create(struct lorie_server_callbacks* callbacks, void *callback_data) {
	struct lorie_server *s = calloc(1, sizeof(struct lorie_server));
	if (s == NULL) {
		LOGE("Can not allocate lorie_server: %m");
		goto fail;
	}
	
	s->display = wl_display_create();
	#if 0
	if (wl_display_add_socket(s->display, "lorie")) {
			LOGE("fatal: failed to add socket\n");
			goto fail;
	}
	#else
	int fd = lorie_create_server_socket();
	if (fd < 0) {
		LOGE("Failed to create server socket for Lorie");
		goto fail;
	}
	
	if (wl_display_add_socket_fd(s->display, fd) != 0) {
		LOGE("Failed to add Lorie server socket to wayland-server runtime");
		goto fail;
	}
	
	s->server_fd = fd;
	#endif
	
	s->lorie_input_global = wl_global_create(s->display, &lorie_input_interface, 1, s, lorie_input_bind);
	if (s->lorie_input_global == NULL) goto fail;
	
	s->lorie_output_global = wl_global_create(s->display, &lorie_output_interface, 1, s, lorie_output_bind);
	if (s->lorie_output_global == NULL) goto fail;
	
	s->callbacks = callbacks;
	s->callback_data = callback_data;
	
	if (s->callbacks != NULL && s->callbacks->client_created != NULL) {
		wl_display_add_client_created_listener(s->display, &s->new_client_listener);
		s->new_client_listener.notify = lorie_display_client_created_handler;
	}
	
	return s;
fail:
	if (s != NULL) {
		if (s->surface != NULL)
			lorie_buffer_fd_destroy(&s->surface);
		if (s->display != NULL)
			wl_display_terminate(s->display);
		free(s);
	}
	return NULL;
}

void *lorie_server_resize_image(struct lorie_server *s, int32_t width, int32_t height) {
	if (s == NULL) return NULL;
	if (s->surface != NULL) lorie_buffer_fd_destroy(&s->surface);
	s->surface = lorie_buffer_fd_create(width, height);
	if (s->surface == NULL) return NULL;
	
	if (!wl_list_empty(wl_display_get_client_list(s->display)))
		lorie_output_send_buffer_change_fd(
			s->lorie_output_implementation,
			lorie_buffer_fd_fd(s->surface),
			lorie_buffer_fd_width(s->surface),
			lorie_buffer_fd_height(s->surface) );
	
	return lorie_buffer_fd_data(s->surface);
}

void lorie_server_send_current_buffer(struct lorie_server *s) {
	if (s == NULL || s->surface == NULL) return;
	if (!wl_list_empty(wl_display_get_client_list(s->display)))
		lorie_output_send_buffer_change_fd(
			s->lorie_output_implementation,
			lorie_buffer_fd_fd(s->surface),
			lorie_buffer_fd_width(s->surface),
			lorie_buffer_fd_height(s->surface) );
}

int lorie_server_get_fd(struct lorie_server *s) {
	if (s == NULL) return -1;
	return s->server_fd;
}

void lorie_server_process(struct lorie_server *s) {
	if (s == NULL) return;
	// wl_display_run blocks the thread. We need to avoid that
	// wl_display_run(s->display);
	wl_display_flush_clients(s->display);
	wl_event_loop_dispatch(wl_display_get_event_loop(s->display), 0);
}

void lorie_server_send_damage(struct lorie_server *s, int32_t x, int32_t y, int32_t w, int32_t h) {
	if (s == NULL) return;
	
	if (wl_list_empty(wl_display_get_client_list(s->display))) return;
	lorie_output_send_damage(s->lorie_output_implementation, x, y, w, h);
}

void lorie_server_destroy(struct lorie_server** server) {
	if (server == NULL) return;
	struct lorie_server* s = *server;
	if (s != NULL) {
		if (s->display != NULL) {
			wl_display_destroy_clients(s->display);
			wl_display_destroy(s->display);
		}
		free(s);
	}
	*server = NULL;
}

