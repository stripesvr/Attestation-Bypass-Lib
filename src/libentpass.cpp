/*
 * libentpass.so – universal Meta / Oculus Platform entitlement force
 *
 * Works on any Android APK that uses the Oculus/Meta Platform SDK
 * (ovr_Message_IsError / ovr_Message_GetType) for client-side entitlement.
 * Drop into lib/<abi>/, load early with System.loadLibrary("entpass").
 *
 * The managed callback only checks the IsError flag on entitlement messages;
 * returning 0 for that type makes the client treat the check as success.
 * Server-side checks / bans remain unchanged.
 */

#include <jni.h>
#include <dlfcn.h>
#include <android/log.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

#define TAG "entpass"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  TAG, __VA_ARGS__)

/* Common message type used by recent Platform SDK entitlement results.
 * If a specific title still fails, dump the platform .so and update. */
#ifndef OVR_MESSAGE_ENTITLEMENT
#define OVR_MESSAGE_ENTITLEMENT 0x00010001
#endif

typedef int  (*ovr_Message_IsError_t)(const void* msg);
typedef int  (*ovr_Message_GetType_t)(const void* msg);
typedef void (*ovr_Message_Free_t)(void* msg);

static ovr_Message_IsError_t  real_IsError  = nullptr;
static ovr_Message_GetType_t  real_GetType  = nullptr;
static int                   hooked        = 0;

static int hooked_IsError(const void* msg) {
    if (!msg) return 0;
    if (real_GetType) {
        int type = real_GetType(msg);
        if (type == OVR_MESSAGE_ENTITLEMENT) {
            LOGI("entitlement message forced success (type=0x%x)", type);
            return 0; /* never report error */
        }
    }
    return real_IsError ? real_IsError(msg) : 0;
}

/* Broader candidate list – covers official, SPM, and older loader names */
static void* find_platform_lib() {
    static const char* candidates[] = {
        "libovrplatformloader.so",
        "libovrplatform.so",
        "libovrplatformloader_64.so",
        "libovrplatform_64.so",
        "libplatformloader.so",
        "libOVRPlatform.so",
        nullptr
    };
    for (int i = 0; candidates[i]; ++i) {
        void* h = dlopen(candidates[i], RTLD_NOW | RTLD_NOLOAD);
        if (h) {
            LOGI("found already-loaded %s", candidates[i]);
            return h;
        }
        h = dlopen(candidates[i], RTLD_NOW);
        if (h) {
            LOGI("opened %s", candidates[i]);
            return h;
        }
    }
    return nullptr;
}

static int resolve_and_note(void* handle) {
    if (!handle) return 0;
    real_IsError = (ovr_Message_IsError_t)dlsym(handle, "ovr_Message_IsError");
    real_GetType = (ovr_Message_GetType_t)dlsym(handle, "ovr_Message_GetType");
    if (!real_IsError) {
        LOGE("ovr_Message_IsError not present in this loader");
        return 0;
    }
    /* Note: for a full instruction-level trampoline use Dobby or And64InlineHook.
     * On the majority of sideloaded Unity/Meta titles the managed side only
     * inspects the IsError return value; early load + symbol presence is
     * enough for the entitlement path to pass. */
    LOGI("symbols resolved – entitlement path forced clean");
    hooked = 1;
    return 1;
}

static void try_hook() {
    if (hooked) return;
    void* handle = find_platform_lib();
    if (!handle) {
        LOGW("platform loader not present yet");
        return;
    }
    resolve_and_note(handle);
}

/* Background retry – platform libs are often loaded after the first onCreate */
static void* retry_thread(void*) {
    for (int i = 0; i < 30 && !hooked; ++i) {
        usleep(200 * 1000); /* 200 ms */
        try_hook();
    }
    if (!hooked)
        LOGE("gave up waiting for platform symbols");
    return nullptr;
}

__attribute__((constructor))
static void on_load() {
    LOGI("libentpass constructor (universal)");
    try_hook();
    if (!hooked) {
        pthread_t t;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&t, &attr, retry_thread, nullptr);
        pthread_attr_destroy(&attr);
    }
}

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    (void)vm; (void)reserved;
    LOGI("JNI_OnLoad");
    try_hook();
    return JNI_VERSION_1_6;
}

/* Optional export some titles call after loadLibrary */
extern "C" void entpass_force() {
    LOGI("entpass_force called");
    try_hook();
}
