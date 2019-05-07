#include <unistd.h>
#include <lorie-server.h>
#include <lorie-log.h>

static void lorie_client_created_handler(void *data, int32_t fd) {
	LOGI("client_created: fd %d", fd);
}

static void lorie_client_destroyed_handler(void *data, int32_t fd) {
	LOGI("client_destroyed: fd %d", fd);
}

static void lorie_pointer_event_handler(void *data, int32_t state, int32_t button, int32_t x, int32_t y) {
	LOGI("pointer_event(%d, %d, %d, %d);", state, button, x, y);
}

static void lorie_key_event_handler(void *data, int32_t state, int32_t key) {
	LOGI("key_event(%d, %d);", state, key);
}

static void lorie_resolution_change_handler(void *data, int32_t x, int32_t y) {
	LOGI("resolution_change(%d, %d);", x, y);
}

static struct lorie_server_callbacks server_callbacks = {
	lorie_client_created_handler,
	lorie_client_destroyed_handler,
	
	lorie_pointer_event_handler,
	lorie_key_event_handler,
	lorie_resolution_change_handler
};

int main(void) {	
	struct lorie_server* s = lorie_server_create(&server_callbacks, NULL);
	if (s == NULL) {
		LOGE("error allocating lorie_server\n");
		return 1;
	}
	
	LOGV("s = %p", s);
	
	int a = 100;
	while (a--) {
		sleep (1);
		lorie_server_process(s);
		lorie_server_send_damage(s, 0, 0, 800, 600);
	}
	
	lorie_server_destroy(&s);
	return 0;
}
