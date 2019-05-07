#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 999
#define __USE_MISC
#define __USE_XOPEN_EXTENDED
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/errno.h>
#include <string.h>

#include <lorie-log.h>
#include <lorie-buffer.h>

#ifdef __ANDROID__
#define __u32 uint32_t
#include <linux/ashmem.h>
#endif

struct lorie_buffer {
	int32_t fd, width, height;
	size_t size;
	void *data;
};

#ifndef __ANDROID__
static inline int memfd_create(const char *name, unsigned int flags) {
    return syscall(__NR_memfd_create, name, flags);
}
#endif

struct lorie_buffer* lorie_buffer_fd_from_received(int32_t fd, int32_t width, int32_t height) {
	struct lorie_buffer *b = calloc(1, sizeof(struct lorie_buffer));
	if (b == NULL || fd == -1 || width <= 0 || height <= 0) goto fail;
	
	b->size = width * height * sizeof(uint32_t);
	b->fd = fd;
	b->width = width;
	b->height = height;

	b->data = mmap(NULL, b->size, PROT_READ|PROT_WRITE, MAP_SHARED, b->fd, 0);
	if (b->data == MAP_FAILED) {
		LOGE("fd %d mmap failed: %m", b->fd);
		goto fail;
	}
	
	return b;
fail:
	lorie_buffer_fd_destroy(&b);
	return NULL;
}

struct lorie_buffer* lorie_buffer_fd_create(int32_t width, int32_t height) {
	struct lorie_buffer *b = calloc(1, sizeof(struct lorie_buffer));
	if (b == NULL) goto fail;
	
	b->size = width * height * sizeof(uint32_t);
	b->fd = -1;
	b->width = width;
	b->height = height;
	
#ifdef __ANDROID__
	b->fd = open("/dev/ashmem", O_RDWR);
	if (b->fd < 0) goto fail;

	int ret = ioctl(b->fd, ASHMEM_SET_NAME, "lorie-buffer");
	if (ret < 0) goto fail;
	ret = ioctl(b->fd, ASHMEM_SET_SIZE, b->size);
	if (ret < 0) goto fail;
#else
    b->fd = memfd_create("lorie-buffer", 0);
    if (b->fd == -1) goto fail;
    ftruncate(b->fd, b->size);
#endif

	b->data = mmap(NULL, b->size, PROT_READ|PROT_WRITE, MAP_SHARED, b->fd, 0);
	if (b->data == MAP_FAILED) {
		LOGE("fd %d mmap failed: %m", b->fd);
		goto fail;
	}
	
	return b;
fail:
	lorie_buffer_fd_destroy(&b);
	return NULL;
}

void *lorie_buffer_fd_data(struct lorie_buffer *b) {
	if (b == NULL) return NULL;
	return b->data;
}

int32_t lorie_buffer_fd_fd(struct lorie_buffer *b) {
	if (b == NULL) return 0;
	return b->fd;
}

int32_t lorie_buffer_fd_width(struct lorie_buffer *b) {
	if (b == NULL) return 0;
	return b->width;
}

int32_t lorie_buffer_fd_height(struct lorie_buffer *b) {
	if (b == NULL) return 0;
	return b->height;
}

void lorie_buffer_fd_destroy(struct lorie_buffer **buffer) {
	if (buffer == NULL || *buffer == NULL) return;
	struct lorie_buffer *b = *buffer;
	if (b->data != NULL) munmap(b->data, b->size);
	if (b->fd != -1) close(b->fd);
	free(b);
}
