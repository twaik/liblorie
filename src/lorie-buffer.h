#pragma once
#include <stdint.h>

struct lorie_buffer;

struct lorie_buffer* lorie_buffer_fd_create(int32_t width, int32_t height);
struct lorie_buffer* lorie_buffer_fd_from_received(int32_t fd, int32_t width, int32_t height);
void *lorie_buffer_fd_data(struct lorie_buffer* b);
int32_t lorie_buffer_fd_fd(struct lorie_buffer *b);
int32_t lorie_buffer_fd_width(struct lorie_buffer *b);
int32_t lorie_buffer_fd_height(struct lorie_buffer *b);
void lorie_buffer_fd_destroy(struct lorie_buffer **b);
