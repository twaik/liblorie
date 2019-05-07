#pragma once
#include <stdint.h>

struct lorie_server;

struct lorie_server_callbacks {
	void (*client_created)(void *data, int32_t fd);
	void (*client_destroyed)(void *data, int32_t fd);
	
	void (*pointer_event)(void *data, int32_t state, int32_t button, int32_t x, int32_t y);
	void (*key_event)(void *data, int32_t state, int32_t key);
	void (*resolution_change)(void *data, int32_t x, int32_t y);
};

struct lorie_server* lorie_server_create(struct lorie_server_callbacks* callbacks, void *callback_data);
int lorie_server_get_fd(struct lorie_server *s);
void lorie_server_process(struct lorie_server *s);
void *lorie_server_resize_image(struct lorie_server *s, int32_t width, int32_t height);
void lorie_server_send_current_buffer(struct lorie_server *s);
void lorie_server_send_damage(struct lorie_server *s, int32_t x, int32_t y, int32_t w, int32_t h);
void lorie_server_destroy(struct lorie_server** server);
