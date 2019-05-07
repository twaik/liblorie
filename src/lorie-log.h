#ifdef __ANDROID__
#include <android/log.h>

#ifndef LOG_TAG
#define LOG_TAG "XLorie"
#endif

#define LOG(prio, ...) __android_log_print(prio, LOG_TAG, __VA_ARGS__)

#define LOGI(...) LOG(ANDROID_LOG_INFO, __VA_ARGS__)
#define LOGW(...) LOG(ANDROID_LOG_WARN, __VA_ARGS__)
#define LOGD(...) LOG(ANDROID_LOG_DEBUG, __VA_ARGS__)
#define LOGV(...) LOG(ANDROID_LOG_VERBOSE, __VA_ARGS__)
#define LOGE(...) LOG(ANDROID_LOG_ERROR, __VA_ARGS__)
#define LOGF(...) LOG(ANDROID_LOG_FATAL, __VA_ARGS__)

#else 
#include <stdio.h>

#define LOG(prio, ...) { printf(prio __VA_ARGS__); printf("\n"); }

#define LOGI(...) LOG("I: " , __VA_ARGS__)
#define LOGW(...) LOG("W: " , __VA_ARGS__)
#define LOGD(...) LOG("D: " , __VA_ARGS__)
#define LOGV(...) LOG("V: " , __VA_ARGS__)
#define LOGE(...) LOG("E: " , __VA_ARGS__)
#define LOGF(...) LOG("F: " , __VA_ARGS__)

#endif 
#ifdef DBG
#undef DBG
#endif

#define DBG LOGD("Here! %s %d", __FILE__, __LINE__)
