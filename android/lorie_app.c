#include <lorie-client.h>
#include <lorie-client-protocol.h>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <EGL/egl.h>

#include <android/window.h>
#include <android/native_window_jni.h>
#include <android/log.h>

//#include <time.h>

#define LOG(p, ...) __android_log_print(p, "LorieNative", __VA_ARGS__)
#define LOGI(...) LOG(ANDROID_LOG_INFO, __VA_ARGS__)
#define LOGE(...) LOG(ANDROID_LOG_ERROR, __VA_ARGS__)

#define DBG LOGE("Here! %d", __LINE__)
static JavaVM *jvm = NULL;

typedef struct {
	int fds[2];
	pthread_mutex_t lock;
} eventQueue;

typedef struct {
    eventQueue q;
    int running;
    
    int connected;
    struct lorie_client *c;
    EGLNativeWindowType pendingWin;
} nativeStuff;

enum messages_t {
	MSG_SURFACE_CREATED = 1,
	MSG_SURFACE_CHANGED, 
	MSG_SURFACE_DESTROYED,
};

nativeStuff* currentRenderer = NULL;

static void taskWrite(eventQueue *q, int t) {
	//pthread_mutex_lock(&q->lock);
	if (write(q->fds[0], &t, sizeof(int)) == -1) LOGE("Error writing: %s", strerror(errno));
	//pthread_mutex_unlock(&q->lock);
}

static int taskRead(eventQueue *q) {
	int size, t;
	if (ioctl(q->fds[1], FIONREAD, &size) == -1) LOGE("IOCTL failed");
	if (size < sizeof(int)) return -1;
	
	//pthread_mutex_lock(&q->lock);
	if (read(q->fds[1], &t, sizeof(int)) == -1) LOGE("Error writing: %s", strerror(errno));
	//pthread_mutex_unlock(&q->lock);
	return t;
}

static void* taskThread(void* data) {
    nativeStuff* s = data;
    int task = 0;
DBG;
	setenv("XDG_RUNTIME_DIR", "/data/data/com.termux/files/usr/tmp/", 1);
    while (s->running) {
		while((task = taskRead(&s->q)) >= 0) {
			switch(task) {
				case MSG_SURFACE_CREATED: DBG;
					if (s->c == NULL) {
						s->c = lorie_client_create(NULL, s->pendingWin);
						if (s->c == NULL) LOGE("Allocating lorie_client failed");
					} else {
						lorie_client_attach_window(s->c, s->pendingWin);
					}
					s->pendingWin = NULL;
					break;
				case MSG_SURFACE_DESTROYED:DBG;
					if (s->c != NULL) {
						lorie_client_attach_window(s->c, NULL);
					}
					break;
			}
		}

		if (s->c != NULL) {
			if (!lorie_client_is_connected(s->c)) {DBG;
				lorie_client_connect(s->c);
				if (lorie_client_is_connected(s->c)) {
					int32_t w = 0, h = 0;
					lorie_client_get_resolution(s->c, &w, &h);
					lorie_client_send_resolution_change_request(s->c, w, h);
				} else {
					usleep(500000);
					continue;
				}
			} else {
				if (lorie_client_sync(s->c, 0) == -1) 
					lorie_client_disconnect(s->c);
			}
		}

		usleep(1000);
    }
    DBG;
    if (s->c != NULL) {DBG;
		lorie_client_destroy(&s->c);
	}
    return NULL;
}

static jlong nativeInit(__unused JNIEnv *env, __unused jobject instance) {
    int ret;
    nativeStuff* ptr = calloc(1, sizeof(nativeStuff));
    if (ptr == NULL) {
		LOGE("Allocating native stuff failed: %m");
	}
    ptr->running = 1;
    
    pthread_mutex_init(&ptr->q.lock, NULL);

    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, ptr->q.fds);
    if (ret) {
        LOGE("Creating socketpair failed: %s", strerror(errno));
        return 0;
    }

    pthread_t thread;
    pthread_create(&thread, NULL, taskThread, ptr);
    
    return (jlong) ptr;
}

static void nativeTouch(__unused JNIEnv *env, __unused jobject instance, jlong jnative, jint button, jint state, jint x, jint y) {
    if (jnative == 0) return;
    nativeStuff *s = (nativeStuff*) jnative;
    if (s != NULL && lorie_client_is_connected(s->c))
		lorie_client_send_pointer_event(s->c, state, 1, x, y);
}

static void nativeSurfaceCreated(__unused JNIEnv *env, __unused jobject instance, jlong jnative, jobject jsurface) {
    if (jnative == 0) return;
    nativeStuff *s = (nativeStuff*) jnative;
    s->pendingWin = ANativeWindow_fromSurface(env, jsurface);
    taskWrite(&s->q, MSG_SURFACE_CREATED);
}

static void nativeSurfaceChanged(__unused JNIEnv *env, __unused jobject instance, jlong jnative, jobject jsurface, jint width, jint height) {
    if (jnative == 0) return;

    nativeStuff *s = (nativeStuff*) jnative;
    //taskWrite(&s->q, MSG_SURFACE_CHANGED);
    if (s != NULL && lorie_client_is_connected(s->c))
		lorie_client_send_resolution_change_request(s->c, width, height);
}

static void nativeSurfaceDestroyed(__unused JNIEnv *env, __unused jobject instance, jlong jnative, jobject jsurface) {
    if (jnative == 0) return;

    nativeStuff *s = (nativeStuff*) jnative;
    taskWrite(&s->q, MSG_SURFACE_DESTROYED);
}

static JNINativeMethod methods[] = {
        {"nativeInit", "()J", (void*)nativeInit },
        {"nativeTouch", "(JIIII)V", (void*)nativeTouch },
        {"onSurfaceCreated", "(JLandroid/view/Surface;)V", (void*)nativeSurfaceCreated},
        {"onSurfaceChanged", "(JLandroid/view/Surface;II)V", (void*)nativeSurfaceChanged},
        {"onSurfaceDestroyed", "(JLandroid/view/Surface;)V", (void*)nativeSurfaceDestroyed }
};

static const char *className = "com/termux/Lorie/NativeSurfaceView";
JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, __unused void* reserved) {
    JNIEnv *env;
    jclass cls;

    if ((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_6) != JNI_OK) {
        LOGE("ERROR: GetEnv failed");
        return JNI_FALSE;
    }

    cls = (*env)->FindClass(env, className);
    if (cls == NULL) {
        LOGE("Native registration unable to find class '%s'", className);
        return JNI_FALSE;
    }

    if ((*env)->RegisterNatives(env, cls, methods, sizeof(methods) / sizeof(methods[0])) < 0) {
        LOGE("RegisterNatives failed for '%s'", className);
        return JNI_FALSE;
    }

    if((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        return 0;
    }

    jvm = vm;

    return JNI_VERSION_1_6;
}

